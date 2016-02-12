#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

#include <hobbes_enclave.h>
#include <hobbes_app.h>
#include <hobbes_db.h>

#include <pet_log.h>
#include <petos.h>

#include "hio.h"

/* 512GB: stack base address */
#define SMARTMAP_ALIGN  (1ULL << 39)
#define HIO_STACK_BASE	SMARTMAP_ALIGN		


/* HIO regions */
struct hio_region {
    uintptr_t     base_addr_va;
    uintptr_t     base_addr_pa;
    uint64_t      size;

    void        * local_va;
    xemem_segid_t segid;
};

static struct hio_region hio_data;
static struct hio_region hio_heap;
static struct hio_region hio_stack;



static void *
hio_map_region(uintptr_t base_addr,
               uint64_t  size)
{
    int    fd   = 0;
    void * addr = NULL;

    if (size % PAGE_SIZE_4KB) {
	ERROR("Cannot map HIO region of size %lu: must be multiple of PAGE_SIZE (%llu)\n",
		size, PAGE_SIZE_4KB);
	return NULL;
    }

    fd = open(PETOS_DEV_FILE, O_RDWR);
    if (fd == -1) {
	ERROR("Cannot open %s: %s. Cannot map memory for HIO region\n",
		PETOS_DEV_FILE, strerror(errno));
	return NULL;
    }

    addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (unsigned long)base_addr);

    close(fd);

    if (addr == MAP_FAILED) {
	ERROR("Cannot mmap %s: %s. Cannot map memory for HIO region\n",
		PETOS_DEV_FILE, strerror(errno));
	return NULL;
    }

    return addr;
}

static int
hio_unmap_region(void   * vaddr,
		 uint64_t size)
{
    if (size % PAGE_SIZE_4KB) {
	ERROR("Cannot unmap HIO region of size %lu: must be multiple of PAGE_SIZE (%llu)\n",
		size, PAGE_SIZE_4KB);
	return -1;
    }

    return munmap(vaddr, size);
}

static int
hio_export_region(uintptr_t       pa,
		  uint64_t        size,
	          void         ** va_p,
		  xemem_segid_t * segid_p)
{
    xemem_segid_t segid = XEMEM_INVALID_SEGID;
    void        * va    = NULL;

    va = hio_map_region(pa, size);
    if (va == NULL) {
	ERROR("Cannot map HIO region\n");
	goto map_out;
    }

    segid = xemem_make(va, size, NULL);
    if (segid == XEMEM_INVALID_SEGID) {
	ERROR("Cannot export HIO region\n");
	goto export_out;
    }

    *va_p    = va;
    *segid_p = segid;

    return 0;

export_out:
    hio_unmap_region(va, size);

map_out:
    return -1;
}


static void
hio_set_heap_base_address(uintptr_t   data_va,
			  uint64_t    data_size,
			  uint32_t    page_size,
			  uintptr_t * heap_va)
{
    /* Kitten sets the heap at the first page aligned address after the LOAD segment */
    *heap_va = PAGE_ALIGN_UP(data_va + data_size, page_size);
}

static void
hio_set_stack_base_address(uint64_t    stack_size,
			   uintptr_t * stack_va)
{
    *stack_va = HIO_STACK_BASE - stack_size;
}

/* Build spaces-delimited argv string */
static int
hio_prepare_argv(uintptr_t     data_va,
		 uint64_t      data_size,
		 xemem_segid_t data_segid,
		 uintptr_t     heap_va,
		 uint64_t      heap_size,
		 xemem_segid_t heap_segid,
		 uintptr_t     stack_va,
		 uint64_t      stack_size,
		 xemem_segid_t stack_segid,
		 char        * original_argv_str,
		 char       ** new_hio_argv_str)
{
    char * argv      = NULL;
    char tmp_str[64] = { 0 };

    /* Data */
    {
	snprintf(tmp_str, 64, "%lx", data_va);
	asprintf(&argv, "%s", tmp_str);

	snprintf(tmp_str, 64, "%lx", data_size);
	asprintf(&argv, "%s %s", argv, tmp_str);

	snprintf(tmp_str, 64, "%li", data_segid);
	asprintf(&argv, "%s %s", argv, tmp_str);
    }

    /* Heap */
    {
	snprintf(tmp_str, 64, "%lx", heap_va);
	asprintf(&argv, "%s %s", argv, tmp_str);

	snprintf(tmp_str, 64, "%lx", heap_size);
	asprintf(&argv, "%s %s", argv, tmp_str);

	snprintf(tmp_str, 64, "%li", heap_segid);
	asprintf(&argv, "%s %s", argv, tmp_str);
    }

    /* Stack */
    {
	snprintf(tmp_str, 64, "%lx", stack_va);
	asprintf(&argv, "%s %s", argv, tmp_str);

	snprintf(tmp_str, 64, "%lx", stack_size);
	asprintf(&argv, "%s %s", argv, tmp_str);

	snprintf(tmp_str, 64, "%li", stack_segid);
	asprintf(&argv, "%s %s", argv, tmp_str);
    }

    /* Original argv */
    if (original_argv_str)
	asprintf(&argv, "%s %s", argv, original_argv_str);

    *new_hio_argv_str = argv;
    return 0;
}

hobbes_app_spec_t
hobbes_init_hio_app(hobbes_id_t hio_app_id,
		    char      * name,
		    char      * exe_path,
		    char      * hio_exe_path,
		    char      * hio_argv,
		    char      * hio_envp,
		    uint64_t    page_size,
		    uintptr_t   data_va,
		    uintptr_t   data_pa,
		    uintptr_t   heap_pa,
		    uintptr_t   stack_pa,
		    uint64_t    data_size,
		    uint64_t	heap_size,
		    uint64_t	stack_size)
{
    uintptr_t heap_va   = 0;
    uintptr_t stack_va  = 0;

    void * data_local_va  = NULL;
    void * heap_local_va  = NULL;
    void * stack_local_va = NULL;

    xemem_segid_t data_segid  = XEMEM_INVALID_SEGID;
    xemem_segid_t heap_segid  = XEMEM_INVALID_SEGID;
    xemem_segid_t stack_segid = XEMEM_INVALID_SEGID;

    hobbes_app_spec_t spec   = NULL;
    int               status = 0;

    /* (1) VA base addresses and sizes */
    {
	/* Data va/size passed in as parameter now */

	/* Set heap base address */
	hio_set_heap_base_address(data_va, data_size, page_size, &heap_va);

	/* Set stack base address */
	hio_set_stack_base_address(stack_size, &stack_va);
    }

    /* (2) Export PA regions via XEMEM */
    {
	status = hio_export_region(
			data_pa,
			data_size,
			&data_local_va,
			&data_segid);
	if (status) {
	    ERROR("Could not export HIO data region\n");
	    goto out_data;
	}

	status = hio_export_region(
			heap_pa,
			heap_size,
			&heap_local_va,
			&heap_segid);
	if (status) {
	    ERROR("Could not export HIO heap region\n");
	    goto out_heap;
	}

	status = hio_export_region(
			stack_pa,
			stack_size,
			&stack_local_va,
			&stack_segid);
	if (status) {
	    ERROR("Could not export HIO stack region\n");
	    goto out_stack;
	}
    }

    /* (3) Create app */
    {
	char * stub_argv = NULL;

	/* Prepend ARGV with region specifications */
	hio_prepare_argv(
		data_va,
		data_size,
		data_segid,
		heap_va,
		heap_size,
		heap_segid,
		stack_va,
		stack_size,
		stack_segid,
		hio_argv,
		&stub_argv);

	spec = hobbes_build_app_spec(
		hio_app_id,
		name,
		hio_exe_path,
		stub_argv,
		hio_envp,
		NULL, /* cpu list */
		0, /* use large pages */
		0, /* use smartmap */
		1, /* num ranks */
		0, /* data size */
		0, /* heap size */
		0, /* stack size */
		1, /* use prealloc mem */
		0, /* data pa */
		0, /* heap pa */
		0  /* stack pa */
	);

	free(stub_argv);

	if (!spec) {
	    ERROR("Could not prepare HIO app specification\n");
	    goto out_spec;
	}
    }

    printf("HIO region spec for stub:\n"
	"\tData:\n"
	"\t\tVA: [0x%lx, 0x%lx)\n"
	"\t\tPA: [0x%lx, 0x%lx)\n"
	"\t\tXEMEM segid: %li\n"
	"\tHeap:\n"
	"\t\tVA: [0x%lx, 0x%lx)\n"
	"\t\tPA: [0x%lx, 0x%lx)\n"
	"\t\tXEMEM segid: %li\n"
	"\tStack:\n"
	"\t\tVA: [0x%lx, 0x%lx)\n"
	"\t\tPA: [0x%lx, 0x%lx)\n"
	"\t\tXEMEM segid: %li\n",
	data_va, (data_va + data_size),
	data_pa, (data_pa + data_size),
	data_segid,
	heap_va, (heap_va + heap_size),
	heap_pa, (heap_pa + heap_size),
	heap_segid,
	stack_va, (stack_va + stack_size),
	stack_pa, (stack_pa + stack_size),
	stack_segid);

    /* Save region info */
    hio_data.base_addr_va  = data_va;
    hio_data.base_addr_pa  = data_pa;
    hio_data.size          = data_size;
    hio_data.local_va      = data_local_va;
    hio_data.segid         = data_segid;

    hio_heap.base_addr_va  = heap_va;
    hio_heap.base_addr_pa  = heap_pa;
    hio_heap.size          = heap_size;
    hio_heap.local_va      = heap_local_va;
    hio_heap.segid         = heap_segid;

    hio_stack.base_addr_va = stack_va;
    hio_stack.base_addr_pa = stack_pa;
    hio_stack.size         = stack_size;
    hio_stack.local_va     = stack_local_va;
    hio_stack.segid        = stack_segid;

    return spec;

out_spec:
    xemem_remove(stack_segid);
    hio_unmap_region(stack_local_va, stack_size);

out_stack:
    xemem_remove(heap_segid);
    hio_unmap_region(heap_local_va, heap_size);

out_heap:
    xemem_remove(data_segid);
    hio_unmap_region(data_local_va, data_size);

out_data:
    return NULL;
}

int
hobbes_deinit_hio_app(void)
{
    xemem_remove(hio_stack.segid);
    hio_unmap_region(hio_stack.local_va, hio_stack.size);

    xemem_remove(hio_heap.segid);
    hio_unmap_region(hio_heap.local_va, hio_heap.size);

    xemem_remove(hio_data.segid);
    hio_unmap_region(hio_data.local_va, hio_data.size);

    return 0;
}
