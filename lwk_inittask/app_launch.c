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
#include <lwk/liblwk.h>
#include <arch/types.h>
#include <lwk/pmem.h>
#include <lwk/smp.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <poll.h>

#include <stdint.h>


#include <pet_log.h>
#include <ezxml.h>

#include <hobbes_util.h>
#include <hobbes_cmd_queue.h>

extern cpu_set_t enclave_cpus;

#include "pisces.h"
#include "app_launch.h"

#define DEFAULT_NUM_RANKS       (1)
#define DEFAULT_CPU_MASK        (~0x0ULL)
#define DEFAULT_USE_LARGE_PAGES (0)
#define DEFAULT_USE_SMARTMAP    (0)
#define DEFAULT_HEAP_SIZE       (16 * 1024 * 1024)
#define DEFAULT_STACK_SIZE      (256 * 1024)
#define DEFAULT_ENVP            ""
#define DEFAULT_ARGV            ""



static char * 
get_val(ezxml_t   cfg,
	char    * tag) 
{
    char   * attrib = (char *)ezxml_attr(cfg, tag);
    ezxml_t  txt    = ezxml_child(cfg, tag);
    char   * val    = NULL;

    if ((txt != NULL) && (attrib != NULL)) {
	ERROR("Invalid Cfg file: Duplicate value for %s (attr=%s, txt=%s)\n", 
	       tag, attrib, ezxml_txt(txt));
	return NULL;
    }

    val = (attrib == NULL) ? ezxml_txt(txt) : attrib;

    /* An non-present value actually == "". So we check if the 1st char is '/0' and return NULL */
    if (!*val) return NULL;

    return val;
}


int 
launch_app(char        * name, 
	   char        * exe_path, 
	   char        * argv, 
	   char        * envp, 
	   job_flags_t   flags,
	   uint8_t       num_ranks, 
	   uint64_t      cpu_mask,
	   uint64_t      heap_size,
	   uint64_t      stack_size)
{
    uint32_t page_size = (flags.use_large_pages ? VM_PAGE_2MB : VM_PAGE_4KB);

    vaddr_t        file_addr = 0;
    cpu_set_t      spec_cpus;
    cpu_set_t      job_cpus;
    user_cpumask_t lwk_cpumask;

    int status = 0;


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

	if (CPU_COUNT(&job_cpus) < num_ranks) {
	    printf("Error: Could not find enough CPUs for job\n");
	    return -1;
	}
	

	cpus_clear(lwk_cpumask);
	
	for (i = 0; (i < CPU_MAX_ID) && (i < CPU_SETSIZE); i++) {
	    if (CPU_ISSET(i, &job_cpus)) {
		cpu_set(i, lwk_cpumask);
	    }
	}
    }


    /* Load exe file info memory */
    {
	size_t file_size = 0;
	id_t   my_aspace_id;

	status = aspace_get_myid(&my_aspace_id);
	if (status != 0) 
	    return status;

	file_size = pisces_file_stat(exe_path);

	{
	
	    paddr_t pmem = elf_dflt_alloc_pmem(file_size, page_size, 0);

	    printf("PMEM Allocated at %p (file_size=%lu) (page_size=0x%x) (pmem_size=%p)\n", 
		   (void *)pmem, 
		   file_size,
		   page_size, 
		   (void *)round_up(file_size, page_size));

	    if (pmem == 0) {
		printf("Could not allocate space for exe file\n");
		return -1;
	    }

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
	    if (status)
		return status;
	
	}
    
	printf("Loading EXE into memory\n");
	pisces_file_load(exe_path, (void *)file_addr);
    }


    printf("Job Launch Request (%s) [%s %s]\n", name, exe_path, argv);


    /* Initialize start state for each rank */
    {
	start_state_t * start_state = NULL;
	int rank = 0; 

	/* Allocate start state for each rank */
	start_state = malloc(num_ranks * sizeof(start_state_t));
	if (!start_state) {
	    printf("malloc of start_state[] failed\n");
	    return -1;
	}


	for (rank = 0; rank < num_ranks; rank++) {
	    int cpu = 0;
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
	    start_state[rank].cpu_id   = ANY_ID;
	    start_state[rank].user_id  = 1;
	    start_state[rank].group_id = 1;
	    
	    sprintf(start_state[rank].task_name, name);


	    status = elf_load((void *)file_addr,
			      name,
			      ANY_ID,
			      page_size, 
			      heap_size,   // heap_size 
			      stack_size,  // stack_size 
			      argv,        // argv_str
			      envp,        // envp_str
			      &start_state[rank],
			      0,
			      &elf_dflt_alloc_pmem
			      );
	    

	    if (status) {
		printf("elf_load failed, status=%d\n", status);
	    }
	    

	    if ( aspace_update_user_cpumask(start_state[rank].aspace_id, &lwk_cpumask) != 0) {
		printf("Error updating CPU mask\n");
		return -1;
	    }
		 

	    /* Setup Smartmap regions if enabled */
	    if (flags.use_smartmap) {
		int src = 0;
		int dst = 0;

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
			    return -1;
			}
		    }
		}
		printf("    OK\n");
	    }

	    
	    printf("Creating Task\n");
	    
	    status = task_create(&start_state[rank], NULL);
	}
    }
    
    return 0;
}



int 
launch_app_spec(char * spec_str)
{
    ezxml_t   spec      = NULL;
    char    * parse_str = NULL;
    int       ret       = -1;    /* This is only set to 0 if the function completes successfully */

    parse_str = strdup(spec_str);

    spec = ezxml_parse_str(spec_str, strlen(spec_str) + 1);

    if (!spec) {
	ERROR("Invalid App spec\n");
	goto out;
    }

    {
	char        * name       = NULL; 
	char        * exe_path   = NULL; 
	char        * argv       = DEFAULT_ARGV;
	char        * envp       = DEFAULT_ENVP; 
	job_flags_t   flags      = {0};
	uint8_t       num_ranks  = DEFAULT_NUM_RANKS; 
	uint64_t      cpu_mask   = DEFAULT_CPU_MASK;
	uint64_t      heap_size  = DEFAULT_HEAP_SIZE;
	uint64_t      stack_size = DEFAULT_STACK_SIZE;
	
	char * val_str = NULL;


	/* Executable Path Name */
	val_str = get_val(spec, "path");

	if (val_str) {
	    exe_path = val_str;
	} else {
	    ERROR("Missing required path in Hobbes APP specification\n");
	    goto out;
	}

	/* Process Name */
	val_str = get_val(spec, "name");

	if (val_str) {
	    name = val_str;
	} else {
	    name = exe_path;
	}


	/* ARGV */
	val_str = get_val(spec, "argv");

	if (val_str) {
	    argv = val_str;
	}

	/* ENVP */
	val_str = get_val(spec, "envp");

	if (val_str) {
	    envp = val_str;
	}

	/* Ranks */
	val_str = get_val(spec, "ranks");
	
	if (val_str) { 
	    num_ranks = smart_atoi(DEFAULT_NUM_RANKS, val_str);
	}
	
	/* CPU Mask */
	val_str = get_val(spec, "cpu_list");
	
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
	val_str = get_val(spec, "use_large_pages");

	if (val_str) {
	    flags.use_large_pages = DEFAULT_USE_LARGE_PAGES;
	}


	/* Use SmartMap */
	val_str = get_val(spec, "use_smartmap");

	if (val_str) {
	    flags.use_smartmap = DEFAULT_USE_SMARTMAP;
	}

	/* Heap Size */
	val_str = get_val(spec, "heap_size");
	
	if (val_str) {
	    heap_size = smart_atoi(DEFAULT_HEAP_SIZE, val_str);
	}

	/* Stack Size */
	val_str = get_val(spec, "stack_size");

	if (val_str) {
	    stack_size = smart_atoi(DEFAULT_STACK_SIZE, val_str);
	}


	/* Launch App */
	ret = launch_app(name, 
			 exe_path, 
			 argv,
			 envp,
			 flags,
			 num_ranks,
			 cpu_mask,
			 heap_size,
			 stack_size);

	if (ret == -1) {
	    ERROR("Failed to Launch application (spec_str=[%s])\n", spec_str);
	    goto out;
	}
    }


 out:
    ezxml_free(spec);
    free(parse_str);
    return ret;
}
