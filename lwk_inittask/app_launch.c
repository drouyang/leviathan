/* Kitten Job Launch 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <arch/types.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>


#include <lwk/liblwk.h>
#include <lwk/pmem.h>
#include <lwk/smp.h>

#include <stdint.h>

#include <pet_log.h>
#include <pet_xml.h>
#include <pet_hashtable.h>

#include <hobbes.h>
#include <hobbes_app.h>
#include <hobbes_db.h>
#include <hobbes_util.h>
#include <hobbes_notifier.h>

extern hdb_db_t hobbes_master_db;

extern cpu_set_t enclave_cpus;

#include "pisces.h"
#include "app_launch.h"
#include "init.h"

#define DEFAULT_NUM_RANKS       (1)
#define DEFAULT_CPU_MASK        (~0x0ULL)
#define DEFAULT_USE_LARGE_PAGES (0)
#define DEFAULT_USE_SMARTMAP    (0)
#define DEFAULT_HEAP_SIZE       (16 * 1024 * 1024)
#define DEFAULT_STACK_SIZE      (256 * 1024)
#define DEFAULT_ENVP            ""
#define DEFAULT_ARGV            ""



#define MAX_ARGC 32
#define MAX_ENVC 64


#define LWK_PROCESS_ALIVE  0
#define LWK_PROCESS_EXITED 1



/*
 * The app_htable and proc_htable hash tables track the apps and
 * processes, respectively, that are are hosted by this enclave.
 * Each app that is launched into the enclave has one 'lwk_app_state'
 * structure added to the app_htable, representing the state of
 * the application. Each process that is launched when an app is
 * started (the num_ranks in the app) has one entry added to the
 * proc_htable.
 *
 *   app_htable  key= hpid   val= lwk_app_state_t
 *   proc_htable key= os_pid val= hpid
 */
static struct hashtable * app_htable  = NULL;
static struct hashtable * proc_htable = NULL;


/*
 * Physical memory allocation state tracking structure.
 * This structure keeps track of the physical memory that is associated
 * with each application process. When the process dies, this structure
 * is used to free the physical memory used by the process.
 */
typedef struct pmem_state {
    struct pmem_region elf;
    struct pmem_region data;
    struct pmem_region heap;
    struct pmem_region stack;
} pmem_state_t;


/*
 * Application state tracking structure.
 * One of these structures is created for each application that is launched
 * and added to the app_htable.
 */
typedef struct lwk_app_state {
    hobbes_id_t     hpid;
    job_flags_t     job_flags;
    unsigned int    num_ranks;
    start_state_t * start_state; /* array of num_ranks entries */
    pmem_state_t *  pmem_state;  /* array of num_ranks entries */
    unsigned int    num_exited;
} lwk_app_state_t;


static uint32_t
htable_hash_fn(uintptr_t key)
{
    return pet_hash_ptr(key);
}


static int
htable_eq_fn(uintptr_t key1, uintptr_t key2)
{
    return (key1 == key2);
}


static lwk_app_state_t *
lookup_app_by_hpid(hobbes_id_t hpid)
{
    lwk_app_state_t *app;

    app = (lwk_app_state_t *)pet_htable_search(app_htable, (uintptr_t) hpid);
    if (app == NULL) {
        printf("ERROR: could not find hpid %d in app_htable\n", hpid);
	return NULL;
    }

    return app;
}


static lwk_app_state_t *
lookup_app_by_pid(pid_t pid)
{
    hobbes_id_t hpid;

    hpid = (hobbes_id_t) pet_htable_search(proc_htable, (uintptr_t) pid);
    if (hpid == (uintptr_t) NULL) {
        printf("ERROR: could not find pid %ld in proc_htable\n", (long) pid);
        return NULL;
    }

    return lookup_app_by_hpid(hpid);
}


/*
 * This function stashes away some information about a
 * newly launched app so that it can be retrieved later.
 */
static int
remember_app(hobbes_id_t     hpid,
             job_flags_t     job_flags,
             unsigned int    num_ranks,
             start_state_t * start_state,
             pmem_state_t *  pmem_state)
{
    lwk_app_state_t *app;
    int status;
    unsigned int rank;

    app = malloc(sizeof(lwk_app_state_t));
    if (!app)
        hobbes_panic("Could not allocate new lwk_app_state_t structure\n");

    app->hpid             = hpid;
    app->job_flags        = job_flags;
    app->num_ranks        = num_ranks;
    app->start_state      = start_state;
    app->pmem_state       = pmem_state;
    app->num_exited       = 0;

    status = pet_htable_insert(app_htable, (uintptr_t)hpid, (uintptr_t)app);
    if (status == 0) {
        hobbes_panic("Could not add app %d to the application hash table\n", hpid);
    }

    /* For each process in the app, add an os_pid->hpid reverse mapping entry */
    for (rank = 0; rank < num_ranks; rank++) {
        pid_t pid = (pid_t)app->start_state[rank].aspace_id;
        status = pet_htable_insert(proc_htable,
                                   (uintptr_t)pid,
                                   (uintptr_t)hpid);
        if (status == 0) {
            hobbes_panic("Could not add process %ld to the process hash table\n", (long) pid);
        }

        /*
         * Initialize the "process exited status" to "not exited".
         * HACK: We overload start_state.arg[0] to be a flag indicating if the
         * process has exited or not (0 = not exited, 1 = exited).
        */
        app->start_state[rank].arg[0] = LWK_PROCESS_ALIVE;
    }

    return 0;
}


/* Free all application state tracking state for the specified app */
static int
forget_app(lwk_app_state_t *app)
{
    pid_t pid;
    uintptr_t status;
    unsigned int rank;

    /* Remove the app from the application hash table */
    status = pet_htable_remove(app_htable, (uintptr_t) app->hpid, 0);
    if (status == (uintptr_t) NULL) {
        hobbes_panic("Could not find app %d in the application hash table\n", app->hpid);
    }

    /* Remove all of the app's entries in the process hash table */
    for (rank = 0; rank < app->num_ranks; rank++) {
        pid    = (pid_t)app->start_state[rank].aspace_id;
        status = pet_htable_remove(proc_htable, (uintptr_t) pid, 0);
        if (status == (uintptr_t) NULL) {
            hobbes_panic("Could not find process %ld in the application hash table\n", (long) pid);
        }
    }

    free(app->start_state);
    free(app->pmem_state);
    free(app);

    return 0;
}


/* Free all physical resources used by the specified app (e.g., phys mem) */
static int
clean_up_app(lwk_app_state_t *app)
{
    unsigned int rank;

    /* Destroy/Free all resources that were being used by the app */
    for (rank = 0; rank < app->num_ranks; rank++) {
        if (aspace_destroy(app->start_state[rank].aspace_id)) {
            hobbes_panic("aspace_destroy() failed\n");
        }

        /* If we allocated physical memory for the app, free it */
        if (!app->job_flags.use_prealloc_mem) {
            if (pmem_free_umem(&app->pmem_state[rank].data)) {
                hobbes_panic("pmem_free_umem(data) failed\n");
            }

            if (pmem_free_umem(&app->pmem_state[rank].heap)) {
                hobbes_panic("pmem_free_umem(heap) failed\n");
            }

            if (pmem_free_umem(&app->pmem_state[rank].stack)) {
                hobbes_panic("pmem_free_umem(stack) failed\n");
            }
        }
    }

    /* Free the physical memory storing the ELF exe */
    if (pmem_free_umem(&app->pmem_state[0].elf)) {
        hobbes_panic("pmem_free_umem(elf) failed\n");
    }

    forget_app(app);
    return 0;
}


int
init_lwk_app(void)
{
    app_htable = pet_create_htable(0, htable_hash_fn, htable_eq_fn);
    if (!app_htable) {
        ERROR("Could not create application hash table\n");
        return -1;
    }

    proc_htable = pet_create_htable(0, htable_hash_fn, htable_eq_fn);
    if (!proc_htable) {
        ERROR("Could not create application process table\n");
        return -1;
    }

    /* TODO: call add_fd_handler(fd, reap_children, app) here */

    return 0;
}


int
deinit_lwk_app(void)
{
    pet_free_htable(app_htable, 1, 0);
    pet_free_htable(proc_htable, 0, 0);
    return 0;
}


static int
load_segment_hobbes(void            * elf_image,
		    struct elf_phdr * phdr,
		    id_t              aspace_id,
		    vaddr_t           start,
		    size_t            extent,
		    vmpagesize_t      pagesz,
		    paddr_t	      pmem)
{
    int status;
    vaddr_t local_start = 0;
    vaddr_t src, dst;
    id_t my_aspace_id;

    /* Figure out my address space ID */
    if ((status = aspace_get_myid(&my_aspace_id)))
	return status;

    /* Map the segment into the target address space */
    status =
    aspace_map_region(
	    aspace_id,
	    start,
	    extent,
	    elf_pflags_to_vmflags(phdr->p_flags),
	    pagesz,
	    "ELF",
	    pmem
    );
    if (status)
	return status;


    /* Map the segment into this address space */
    status =
    aspace_map_region_anywhere(
	    my_aspace_id,
	    &local_start,
	    extent,
	    (VM_USER|VM_READ|VM_WRITE),
	    pagesz,
	    "temporary",
	    pmem
    );
    if (status)
	return status;

    /* Copy segment data from ELF image into the target address space
     * (via its temporary mapping in our address space) */
    dst = local_start + (phdr->p_vaddr - start);
    src = (vaddr_t)elf_image + phdr->p_offset;
    memcpy((void *)dst, (void *)src, phdr->p_filesz);
    memset((void *)dst + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);

    /* Unmap the segment from this address space */
    status = aspace_del_region(my_aspace_id, local_start, extent);
    if (status)
	return status;

    return 0;
}

#if 0
static int
load_readonly_segment_hobbes(paddr_t           elf_image_paddr,
			     struct elf_phdr * phdr,
			     id_t              aspace_id,
			     vaddr_t           start,
			     size_t            extent,
			     vmpagesize_t      pagesz)
{
    return aspace_map_region(
	    aspace_id,
	    start,
	    extent,
	    elf_pflags_to_vmflags(phdr->p_flags),
	    pagesz,
	    "ELF (mapped)",
	    elf_image_paddr +
		    round_down(phdr->p_offset, pagesz)
    );
}
#endif

static int
make_region_hobbes(id_t		aspace_id,
		   vaddr_t	start,
		   size_t	extent,
		   vmflags_t	flags,
		   vmpagesize_t pagesz,
		   const char * name,
		   paddr_t	base_addr)
{
    int status;

    status = aspace_map_region(aspace_id, start, extent,
			       flags, pagesz,
			       name, base_addr);
    if (status) {
	ERROR("Failed to map physical memory for %s (status=%d).\n",
		name, status);
	return status;
    }

    return 0;
}

static int
elf_load_executable_hobbes(void	      * elf_image,
			   paddr_t	elf_image_paddr,
			   id_t		aspace_id,
			   vmpagesize_t pagesz,
			   paddr_t	load_base_addr)
{
    struct elfhdr *   ehdr;
    struct elf_phdr * phdr_array;
    struct elf_phdr * phdr;
    size_t            i;
    vaddr_t           start, end, base = UINTPTR_MAX;
    size_t            extent;
    size_t            num_load_segments=0;
    int               status;

    /* Locate the program header array (in this context) */
    ehdr       = elf_image;
    phdr_array = (struct elf_phdr *)(elf_image + ehdr->e_phoff);


    /* Set up a region for each program segment */
    for (i = 0; i < ehdr->e_phnum; i++) {
	phdr = &phdr_array[i];
	if (phdr->p_type != PT_LOAD)
		continue;

	/* Calculate the segment's bounds */
	start  = round_down(phdr->p_vaddr, pagesz);
	end    = round_up(phdr->p_vaddr + phdr->p_memsz, pagesz);
	extent = end - start;

	/* BJK: remember the base address of the first PT_LOAD segment. We
	 * calculate the target physical address by offseting from load_base_addr
	 * with the first segment 
	 */
	if (start < base)
	    base = start;

	/* BJK: We need all segments to be mapped to the specified target load
	 * region, including read only segments. Otherwise, read only memory in
	 * the application will be mapped to an address that cannot be seen by
	 * the HIO stub
	 */
	status =
	load_segment_hobbes(
		elf_image,
		phdr,
		aspace_id,
		start,
		extent,
		pagesz,
		load_base_addr + (start - base)
	);
	if (status) {
	    ERROR("load_segment_hobbes: %d\n", status);
	    return status;
	}
#if 0
	if (phdr->p_flags & PF_W) {
	    /* Writable segments must be copied into the
	     * target address space */
	    status =
	    load_writable_segment_hobbes(
		    elf_image,
		    phdr,
		    aspace_id,
		    start,
		    extent,
		    pagesz,
		    load_base_addr + (start - base)
	    );
	    if (status) {
		ERROR("load_writable_segment_hobbes: %d\n", status);
		return status;
	    }
	} else {
	    /* Read-only segments are mapped directly
	     * from the ELF image */
	    status =
	    load_readonly_segment_hobbes(
		    elf_image_paddr,
		    phdr,
		    aspace_id,
		    start,
		    extent,
		    pagesz
	    );
	    if (status)
		return status;
	}
#endif
	++num_load_segments;
    }

    return (num_load_segments) ? 0 : -ENOENT;
}

static int
elf_load_hobbes(void          * elf_image,
		const char    * name,
		id_t		desired_aspace_id,
		vmpagesize_t	pagesz,
		size_t		heap_size,
		size_t		stack_size,
		char          * argv_str,
		char          * envp_str,
		start_state_t * start_state,
		paddr_t		load_base_addr,
		paddr_t		heap_base_addr,
		paddr_t		stack_base_addr)
{
    int      status;
    char   * argv[MAX_ARGC] = { (char *)name };
    char   * envp[MAX_ENVC];
    id_t     my_aspace_id, aspace_id;
    vaddr_t  heap_start, stack_start, stack_end, stack_ptr;
    vaddr_t  local_stack_start;
    size_t   heap_extent, stack_extent;
    uint32_t hwcap;
    paddr_t  elf_image_paddr;

    if (!elf_image || !start_state)
	return -EINVAL;

    if (elf_init_str_array(MAX_ARGC-1, argv+1, argv_str)) {
	ERROR("Too many ARGV strings.\n");
	return -EINVAL;
    }

    if (elf_init_str_array(MAX_ENVC, envp, envp_str)) {
	ERROR("Too many ENVP strings.\n");
	return -EINVAL;
    }

    if (aspace_virt_to_phys(MY_ID, (vaddr_t)elf_image, &elf_image_paddr)) {
	ERROR("Couldn't determine phys address of ELF image.\n");
	return -EINVAL;
    }

    if ((status = aspace_create(desired_aspace_id, name, &aspace_id))) {
	ERROR("Failed to create aspace (status=%d).\n", status);
	return status;
    }

    /* Load the ELF executable's LOAD segments */
    status =
    elf_load_executable_hobbes(
	    elf_image,       /* where I can access the ELF image */
	    elf_image_paddr, /* where it is in physical memory */
	    aspace_id,       /* the address space to map it into */
	    pagesz,          /* page size to map it with */
	    load_base_addr   /* phys mem to map segments to */
    );
    if (status) {
	ERROR("Failed to load ELF image (status=%d).\n", status);
	goto out_load_executable;
    }

    /* Create the UNIX heap */
    heap_start  = round_up(elf_heap_start(elf_image), pagesz);
    heap_extent = round_up(heap_size, pagesz);
    status =
    make_region_hobbes(
	    aspace_id,
	    heap_start,
	    heap_extent,
	    (VM_USER|VM_READ|VM_WRITE|VM_EXEC|VM_HEAP),
	    pagesz,
	    "heap",
	    heap_base_addr
    );
    if (status) {
	ERROR("Failed to create heap (status=%d).\n", status);
	goto out_make_heap;
    }

    /* Create the stack region */
    stack_end    = SMARTMAP_ALIGN;
    stack_start  = round_down(stack_end - stack_size, pagesz);
    stack_extent = stack_end - stack_start;
    status = 
    make_region_hobbes(
	    aspace_id,
	    stack_start,
	    stack_extent,
	    (VM_USER|VM_READ|VM_WRITE|VM_EXEC),
	    pagesz,
	    "stack",
	    stack_base_addr
    );
    if (status) {
	ERROR("Failed to create stack (status=%d).\n", status);
	goto out_make_stack;
    }

    /* Map the stack region into this address space */
    if ((status = aspace_get_myid(&my_aspace_id)))
	goto out_my_id;

    status =
    aspace_map_region_anywhere(
	    my_aspace_id,
	    &local_stack_start,
	    stack_extent,
	    (VM_USER|VM_READ|VM_WRITE),
	    pagesz,
	    "temporary",
	    stack_base_addr
    );
    if (status) {
	ERROR("Failed to map stack locally (status=%d).\n", status);
	goto out_map_stack;
    }

    /* Initialize the stack */
    status = elf_hwcap(start_state->cpu_id, &hwcap);
    if (status) {
	ERROR("Failed to get hw capabilities (status=%d).\n", status);
	goto out_hwcap;
    }

    status =
    elf_init_stack(
	    elf_image,
	    (void *)local_stack_start,  /* Where I can access it */
	    stack_start,                /* Where it is in target aspace */
	    stack_extent,
	    argv, envp,
	    start_state->user_id, start_state->group_id,
	    hwcap,
	    &stack_ptr
    );
    if (status) {
	ERROR("Failed to initialize stack (status=%d).\n", status);
	goto out_init_stack;
    }

    /* Unmap the segment from this address space */
    status = aspace_del_region(my_aspace_id, local_stack_start, stack_extent);
    if (status)
	goto out_aspace_del;

    start_state->aspace_id   = aspace_id;
    start_state->entry_point = elf_entry_point(elf_image);
    start_state->stack_ptr   = stack_ptr;

    return 0;

out_aspace_del:
out_init_stack:
out_hwcap:
    aspace_unmap_region(my_aspace_id, local_stack_start, stack_extent);

out_map_stack:
out_my_id:
    aspace_unmap_region(aspace_id, stack_start, stack_extent);

out_make_stack:
    aspace_unmap_region(aspace_id, heap_start, heap_extent);

out_make_heap:
out_load_executable:
    aspace_destroy(aspace_id);

    return status;
}


int
launch_lwk_app(hobbes_id_t   hpid,
	       char        * name,
	       char        * exe_path,
	       char        * argv,
	       char        * envp,
	       job_flags_t   flags,
	       unsigned int  num_ranks,
	       uint64_t      cpu_mask,
	       uint64_t      data_size,
	       uint64_t      heap_size,
	       uint64_t      stack_size,
	       uintptr_t     data_pa,
	       uintptr_t     heap_pa,
	       uintptr_t     stack_pa)
{
    uint32_t page_size = (flags.use_large_pages ? VM_PAGE_2MB : VM_PAGE_4KB);

    vaddr_t        file_addr = 0;
    cpu_set_t      spec_cpus;
    cpu_set_t      job_cpus;
    user_cpumask_t lwk_cpumask;

    pmem_state_t * pmem_state;
    start_state_t * start_state;

    int status = 0;

    int error_status = -1;
    unsigned int rank;


    /* Figure out which CPUs are being requested */
    {
	int i = 0;

	CPU_ZERO(&spec_cpus);
	
	for (i = 0; i < 64; i++) {
	    if ((cpu_mask & (0x1ULL << i)) != 0) {
		CPU_SET(i, &spec_cpus);
	    }
	}
    }


    /* Check if we can host the job on the current CPUs */
    /* Create a kitten compatible cpumask */
    {
	int i = 0;

	CPU_AND(&job_cpus, &spec_cpus, &enclave_cpus);

	if (CPU_COUNT(&job_cpus) < (int) num_ranks) {
	    printf("Error: Could not find enough CPUs for job\n");
	    error_status = -1;
            goto error_exit;
	}

	cpus_clear(lwk_cpumask);
	
	for (i = 0; (i < CPU_MAX_ID) && (i < CPU_SETSIZE); i++) {
	    if (CPU_ISSET(i, &job_cpus)) {
		cpu_set(i, lwk_cpumask);
	    }
	}
    }


    /* Allocate a structure to track the physical memory used by the app */
    {
        pmem_state = (pmem_state_t *)calloc(1, num_ranks * sizeof(pmem_state_t));
        if (!pmem_state) {
            printf("ERROR: alloc of pmem_state[] failed\n");
            error_status = -1;
            goto error_exit;
        }

        for (rank = 0; rank < num_ranks; rank++) {
            pmem_region_unset_all(&pmem_state[rank].elf);
            pmem_region_unset_all(&pmem_state[rank].data);
            pmem_region_unset_all(&pmem_state[rank].heap);
            pmem_region_unset_all(&pmem_state[rank].stack);
        }
    }


    /* Load exe file info memory */
    {
	size_t file_size = 0;
	id_t   my_aspace_id;

	status = aspace_get_myid(&my_aspace_id);
	if (status != 0) {
            error_status = status;
            goto error_free_pmem_state;
        }

	file_size = pisces_file_stat(exe_path);

	{
            status = pmem_alloc_umem(file_size, page_size, &pmem_state[0].elf);
            if (status) {
		printf("ERROR: Could not allocate space for exe file\n");
		error_status = -1;
                goto error_free_pmem_state;
            }

            paddr_t pmem = pmem_state[0].elf.start;

	    printf("PMEM Allocated at %p (file_size=%lu) (page_size=0x%x) (pmem_size=%p)\n",
		   (void *)pmem,
		   file_size,
		   page_size,
		   (void *)round_up(file_size, page_size));

	    /* Map the segment into this address space */
	    status =
		aspace_map_region_anywhere(
					   my_aspace_id,
					   &file_addr,
					   round_up(file_size, page_size),
					   (VM_USER|VM_READ|VM_WRITE),
					   page_size,
					   "File",
					   pmem
					   );
	    if (status) {
		error_status = status;
                goto error_free_elf_pmem;
            }
	
	}

	printf("Loading EXE into memory\n");
	pisces_file_load(exe_path, (void *)file_addr);
    }


    printf("Job Launch Request (%s) [%s %s]\n", name, exe_path, argv);


    /* Setup physical memory for each process in the application */
    {
        if (flags.use_prealloc_mem) {
            for (rank = 0; rank < num_ranks; rank++) {
                /* FIXME: Shell should allocate memory for each rank.
                 *        Currently the shell assumes there is only one
                 *        rank per enclave, and only allocates physical
                 *        memory for one process. */
                pmem_state[rank].data.start = data_pa;
                pmem_state[rank].heap.start = heap_pa;
                pmem_state[rank].stack.start = stack_pa;
            }
        } else {
            for (rank = 0; rank < num_ranks; rank++) {
		printf("allocating data_size  = %lu\n", data_size);
		status = pmem_alloc_umem(data_size, page_size, &pmem_state[rank].data);
		if (status) {
                    printf("ERROR: data alloc failed, status=%d\n", status);
                    error_status = status;
                    goto error_free_elf_pmem;
                }

		printf("allocating heap_size  = %lu\n", heap_size);
		status = pmem_alloc_umem(heap_size,  page_size, &pmem_state[rank].heap);
		if (status) {
                    printf("ERROR: heap alloc failed, status=%d\n", status);
                    error_status = status;
                    goto error_free_elf_pmem;
                }

		printf("allocating stack_size = %lu\n", stack_size);
		status = pmem_alloc_umem(stack_size, page_size, &pmem_state[rank].stack);
		if (status) {
                    printf("ERROR: stack alloc failed, status=%d\n", status);
                    error_status = status;
                    goto error_free_elf_pmem;
                }
            }
        }
    }


    /* Initialize start state for each rank */
    {
	/* Allocate start state for each rank */
	start_state = (start_state_t *)malloc(num_ranks * sizeof(start_state_t));
	if (!start_state) {
            printf("ERROR: alloc of pmem_state[] failed\n");
	    status = -1;
            goto error_free_app_pmem;
	}

	for (rank = 0; rank < num_ranks; rank++) {
	    int cpu = ANY_ID;
	    int i   = 0;

	    for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, &job_cpus)) {
		    CPU_CLR(i, &job_cpus);
		    cpu = i;
		    break;
		}
	    }


	    printf("Loading Rank %d on CPU %d\n", rank, cpu);

	    start_state[rank].task_id  = ANY_ID;
	    start_state[rank].cpu_id   = ANY_ID; /* Why does this not work if set to 'cpu'? */
	    start_state[rank].user_id  = 1;
	    start_state[rank].group_id = 1;

	    sprintf(start_state[rank].task_name, "%s-%d", name, rank);

	    char * env_str = NULL;
	    asprintf(&env_str, "%s PMI_RANK=%d PMI_SIZE=%d\n", envp, rank, num_ranks);

            status = elf_load_hobbes((void *)file_addr,
			  name,
			  ANY_ID,
			  page_size,
			  heap_size,   // heap_size
			  stack_size,  // stack_size
			  argv,        // argv_str
			  env_str,     // environment string
			  &start_state[rank],
			  pmem_state[rank].data.start,
			  pmem_state[rank].heap.start,
			  pmem_state[rank].stack.start
	    );

	    free(env_str);

	    if (status) {
		printf("elf_load failed, status=%d\n", status);
                error_status = status;
                goto error_free_aspaces;
	    }

	    if (aspace_update_user_cpumask(start_state[rank].aspace_id, &lwk_cpumask) != 0) {
		printf("Error updating CPU mask\n");
                error_status = -1;
                goto error_free_aspaces;
	    }

	    /* Setup Smartmap regions if enabled */
	    if (flags.use_smartmap) {
		unsigned int src = 0;
		unsigned int dst = 0;

		printf("Creating SMARTMAP mappings...\n");
		for (dst = 0; dst < num_ranks; dst++) {
		    for (src = 0; src < num_ranks; src++) {
			status =
			    aspace_smartmap(
					    start_state[src].aspace_id,
					    start_state[dst].aspace_id,
					    SMARTMAP_ALIGN + (SMARTMAP_ALIGN * src),
					    SMARTMAP_ALIGN
					    );
			
			if (status) {
			    printf("smartmap failed, status=%d\n", status);
			    error_status = -1;
                            goto error_free_aspaces;
			}
		    }
		}
		printf("    OK\n");
	    }

	    printf("Creating Task\n");
	    status = task_create(&start_state[rank], NULL);
	}

        /* Stash away some info about the app that will be needed later */
        remember_app(hpid, flags, num_ranks, start_state, pmem_state);
    }

    return 0;


error_free_aspaces:
    for (rank = 0; rank < num_ranks; rank++) {
        if (start_state[rank].task_id != ANY_ID)
            if (aspace_destroy(start_state[rank].aspace_id))
                hobbes_panic("aspace_destroy() failed\n");
    }
//error_free_start_state:
    free(start_state);
error_free_app_pmem:
    if (flags.use_prealloc_mem) {
        for (rank = 0; rank < num_ranks; rank++) {
            if (pmem_state[rank].data.end && pmem_free_umem(&pmem_state[rank].data))
                hobbes_panic("pmem_free_umem(data) failed\n");
            if (pmem_state[rank].heap.end && pmem_free_umem(&pmem_state[rank].heap))
                hobbes_panic("pmem_free_umem(heap) failed\n");
            if (pmem_state[rank].stack.end && pmem_free_umem(&pmem_state[rank].stack))
                hobbes_panic("pmem_free_umem(stack) failed\n");
        }
    }
error_free_elf_pmem:
    pmem_free_umem(&pmem_state[0].elf);
error_free_pmem_state:
    free(pmem_state);
error_exit:
    return error_status;
}


int
launch_hobbes_lwk_app(char * spec_str)
{
    pet_xml_t   spec  = NULL;
    hobbes_id_t hpid  = HOBBES_INVALID_ID;
    int         ret   = -1;    /* This is only set to 0 if the function completes successfully */



    spec = pet_xml_parse_str(spec_str);

    if (!spec) {
	ERROR("Invalid App spec\n");
	return -1;
    }

    {
	char        * name       = NULL; 
	char        * exe_path   = NULL; 
	char        * argv       = DEFAULT_ARGV;
	char        * envp       = DEFAULT_ENVP; 
	char        * hobbes_env = NULL;
	job_flags_t   flags      = {0};
	unsigned int  num_ranks  = DEFAULT_NUM_RANKS; 
	uint64_t      cpu_mask   = DEFAULT_CPU_MASK;
	uint64_t      data_size  = 0;
	uint64_t      heap_size  = DEFAULT_HEAP_SIZE;
	uint64_t      stack_size = DEFAULT_STACK_SIZE;
	uint64_t      data_pa    = HOBBES_INVALID_ADDR;
	uint64_t      heap_pa    = HOBBES_INVALID_ADDR;
	uint64_t      stack_pa   = HOBBES_INVALID_ADDR;
	
	char        * val_str    = NULL;



	printf("App spec str = (%s)\n", spec_str);

	/* App id */
	val_str = pet_xml_get_val(spec, "app_id");

	hpid = smart_atoi(HOBBES_INVALID_ID, val_str);
	if (hpid == HOBBES_INVALID_ID) {
	    ERROR("Missing required app_id in Hobbes APP specification\n");
	    goto out;
	}


	/* Executable Path Name */
	val_str = pet_xml_get_val(spec, "path");

	if (val_str) {
	    exe_path = val_str;
	} else {
	    ERROR("Missing required path in Hobbes APP specification\n");
	    goto out;
	}

	/* Application Name */
	val_str = pet_xml_get_val(spec, "name");

	if (val_str) {
	    name = val_str;
	} else {
	    name = exe_path;
	}


	/* ARGV */
	val_str = pet_xml_get_val(spec, "argv");

	if (val_str) {
	    argv = val_str;
	}

	/* ENVP */
	val_str = pet_xml_get_val(spec, "envp");

	if (val_str) {
	    envp = val_str;
	}




	/* Ranks */
	val_str = pet_xml_get_val(spec, "ranks");
	
	if (val_str) { 
	    num_ranks = smart_atoi(DEFAULT_NUM_RANKS, val_str);
	}
	
	/* CPU Mask */
	val_str = pet_xml_get_val(spec, "cpu_list");
	
	if (val_str) {
	    char * iter_str = NULL;
	    
	    cpu_mask = 0x0ULL;

	    while ((iter_str = strsep(&val_str, ","))) {
		
		int idx = smart_atoi(-1, iter_str);
		
		if (idx == -1) {
		    printf("Error: Invalid CPU entry (%s)\n", iter_str);
		    goto out;
		}
		
		cpu_mask |= (0x1ULL << idx);
	    }
	}

	
	/* Large page flag */
	val_str = pet_xml_get_val(spec, "use_large_pages");

	if (val_str) {
	    flags.use_large_pages = 1;
	} else {
	    flags.use_large_pages = DEFAULT_USE_LARGE_PAGES;
	}

	/* Use SmartMap */
	val_str = pet_xml_get_val(spec, "use_smartmap");

	if (val_str) {
	    flags.use_smartmap = 1;
	} else {
	    flags.use_smartmap = DEFAULT_USE_SMARTMAP;
	}

	/* Data Size */
	val_str = pet_xml_get_val(spec, "data_size");
	
	if (val_str) {
	    data_size = smart_atoi(0, val_str);
            printf("data_size = %lu\n", data_size);
	}

        if (data_size == 0) {
            ERROR("data_size cannot be 0\n");
            goto out;
        }

	/* Heap Size */
	val_str = pet_xml_get_val(spec, "heap_size");
	
	if (val_str) {
	    heap_size = smart_atoi(DEFAULT_HEAP_SIZE, val_str);
	}

	/* Stack Size */
	val_str = pet_xml_get_val(spec, "stack_size");

	if (val_str) {
	    stack_size = smart_atoi(DEFAULT_STACK_SIZE, val_str);
	}	

	/* Use pre-allocated memory */
	val_str = pet_xml_get_val(spec, "use_prealloc_mem");

	if (val_str) {
	    flags.use_prealloc_mem = 1;

	    val_str  = pet_xml_get_val(spec, "data_base_addr");
	    data_pa  = smart_atou64(HOBBES_INVALID_ADDR, val_str);

	    val_str  = pet_xml_get_val(spec, "heap_base_addr");
	    heap_pa  = smart_atou64(HOBBES_INVALID_ADDR, val_str);

	    val_str  = pet_xml_get_val(spec, "stack_base_addr");
	    stack_pa = smart_atou64(HOBBES_INVALID_ADDR, val_str);

	    if ((data_pa  == HOBBES_INVALID_ADDR) ||
		(heap_pa  == HOBBES_INVALID_ADDR) ||
		(stack_pa == HOBBES_INVALID_ADDR))
	    {
		ERROR("Using preallocated memory, but at least one base address region is corrupt or missing\n");
		goto out;
	    }
	} else {
	    flags.use_prealloc_mem = 0;
	}


	/* Register as a hobbes application */
	{
	    	    
	    int chars_written = 0;

	    printf("Launching App (Hobbes AppID = %u) (EnclaveID=%d)\n", hpid, hobbes_get_my_enclave_id() );
	    printf("Application Name=%s\n", name);

	    /* Hobbes enabled ENVP */
	    chars_written = asprintf(&hobbes_env, 
			   "%s=%u %s=%u %s", 
			   HOBBES_ENV_APP_ID,
			   hpid, 
			   HOBBES_ENV_ENCLAVE_ID,
			   hobbes_get_my_enclave_id(), 
			   envp);

	    if (chars_written == -1) {
		ERROR("Failed to allocate envp string for application (%s)\n", name);
		goto out;
	    }
	    
	    /* Launch App */
	    ret = launch_lwk_app(hpid,
				 name, 
				 exe_path, 
				 argv,
				 hobbes_env,
				 flags,
				 num_ranks,
				 cpu_mask,
				 data_size,
				 heap_size,
				 stack_size,
				 data_pa,
				 heap_pa,
				 stack_pa);

	    free(hobbes_env);

	    if (ret == -1) {
		ERROR("Failed to Launch application (spec_str=[%s])\n", spec_str);
		goto out;
	    }

	    hobbes_set_app_state(hpid, APP_RUNNING);
	    hnotif_signal(HNOTIF_EVT_APPLICATION);
	}
    }

    ret = 0;

 out:
    if ((ret != 0) && (hpid != HOBBES_INVALID_ID)) {
	hobbes_set_app_state(hpid, APP_ERROR);
	hnotif_signal(HNOTIF_EVT_APPLICATION);
    }

    pet_xml_free(spec);
    return ret;
}


int
kill_lwk_app(hobbes_id_t hpid)
{
    lwk_app_state_t *app;
    unsigned int rank;

    app = lookup_app_by_hpid(hpid);
    if (app == NULL) {
        printf("kill_lwk_app() could not find app %d\n", hpid);
	return -1;
    }

    printf("Sending SIGKILL to all processes in app %d\n", hpid);
    for (rank = 0; rank < app->num_ranks; rank++) {
        pid_t pid = (pid_t)app->start_state[rank].aspace_id;

        if (app->start_state[rank].arg[0]) {
            /* the process has already exited, no need to send SIGKILL */
            printf("    rank %u (pid %ld) has already exited\n", rank, (long) pid);
        } else {
	    printf("    sending SIGKILL to rank %u (pid %ld)\n", rank, (long) pid);
            kill(pid, SIGKILL);
        }
    }
    printf("Done, all processes in app %d have been sent a SIGKILL\n", hpid);

    printf("Waiting for all processes in app %d to exit...\n", hpid);
    for (rank = 0; rank < app->num_ranks; rank++) {
        pid_t pid = (pid_t)app->start_state[rank].aspace_id;
	pid_t ret_pid;
        int status;

        if (app->start_state[rank].arg[0]) {
            /* the process has already exited */
            printf("    rank %u (pid %ld) has already exited\n", rank, (long) pid);
            continue;
        } else {
            /* wait for the process to exit */
            if ((ret_pid = waitpid(pid, &status, 0)) == pid) {
                printf("    rank %u (pid %ld) exited, exit_status= %d\n", rank, (long) pid, status);
            } else {
                printf("    rank %u (pid %ld) waitpid returned error %d, errno=%d\n", rank, (long) pid, ret_pid, errno);
            }
        }
    }

    printf("Done, all processes in app %d have exited\n", hpid);
    clean_up_app(app);

    return 0;
}


int
kill_hobbes_lwk_app(hobbes_id_t hpid)
{
    if (kill_lwk_app(hpid) == 0) {
        /* Notify shell that app is dead */
        hobbes_set_app_state(hpid, APP_STOPPED);
        hnotif_signal(HNOTIF_EVT_APPLICATION);
    }

    return 0;
}


int
mark_pid_exited(pid_t pid, int status)
{
    unsigned int rank;
    int found = false;

    lwk_app_state_t *app = lookup_app_by_pid(pid);
    if (!app) {
        printf("ERROR: mark_pid_exited() could not find app for pid %ld\n", (long) pid);
        return -1;
    }

    /* Mark the process as exited */
    app->num_exited += 1;
    for (rank = 0; rank < app->num_ranks; rank++) {
        if (app->start_state[rank].aspace_id == (id_t) pid) {
            printf("    marking app %d rank %u (pid %ld) as exited\n", app->hpid, rank, (long) pid);
            app->start_state[rank].arg[0] = LWK_PROCESS_EXITED;
            app->start_state[rank].arg[1] =status;
            found = true;
            break;
        }
    }
    if (found == false) {
        hobbes_panic("Could not find rank corresponding to pid %ld\n", (long) pid);
    }

/* TODO: fix me */
#if 0
    /* If all processes in the app have exited, destroy the app */
    if (app->num_exited == app->num_ranks) {
        hpid = app->hpid;

        printf("All processes in app %d have exited\n", app->hpid);
        clean_up_app(app);

        /* Notify shell that app is dead */
        hobbes_set_app_state(hpid, APP_STOPPED);
        hnotif_signal(HNOTIF_EVT_APPLICATION);
    }
#endif

    return 0;
}


/* Reap children that have exited. */
int
reap_exited_children(void)
{
    int status;
    pid_t pid;
    unsigned num_exited = 0;

    printf("Reaping all exiting children...\n");

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("    pid %ld exited, exit status was %d\n", (long) pid, status);
        mark_pid_exited(pid, status);
        ++num_exited;
    }

    printf("Done, reaped %u exited children\n", num_exited);

    return 0;
}
