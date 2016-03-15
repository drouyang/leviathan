#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>

#include <hobbes_enclave.h>
#include <hobbes_app.h>
#include <hobbes_db.h>

#include <pet_xml.h>
#include <pet_log.h>
#include <petos.h>

#include "hio.h"

/* 512GB: stack base address */
#define SMARTMAP_ALIGN  (1ULL << 39)
#define HIO_STACK_BASE	SMARTMAP_ALIGN		


/* HIO regions */
struct hio_region {
    uintptr_t     base_addr_va;
    uint64_t      size;

    void        * local_va;
    xemem_segid_t segid;
};

static struct hio_region hio_data;
static struct hio_region hio_heap;
static struct hio_region hio_stack;
static uint32_t          hio_num_ranks;



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
			  uintptr_t * heap_va)
{
    /* Kitten sets the heap at the first page aligned address after the LOAD segment */
    *heap_va = data_va + data_size;
}

static void
hio_set_stack_base_address(uint64_t    stack_size,
			   uintptr_t * stack_va)
{
    *stack_va = HIO_STACK_BASE - stack_size;
}



static int
__hio_prepare_region_specification(pet_xml_t     hio_xml,
				   char        * id,
				   uintptr_t     va,
				   uint64_t      size,
				   xemem_segid_t segid,
				   uint64_t      offset)
{
    pet_xml_t region_xml  = PET_INVALID_XML;
    char      tmp_str[64] = {0};
    int       status      = 0;

    region_xml = pet_xml_add_subtree_tail(hio_xml, "region");
    if (region_xml == PET_INVALID_XML)
	return -1;

    status = pet_xml_add_val(region_xml, "id", id);
    if (status != 0)
	goto out;

    snprintf(tmp_str, 64, "%lu", (uint64_t)va);
    status = pet_xml_add_val(region_xml, "vaddr", tmp_str);
    if (status != 0)
	goto out;

    snprintf(tmp_str, 64, "%lu", size);
    status = pet_xml_add_val(region_xml, "size", tmp_str);
    if (status != 0)
	goto out;

    snprintf(tmp_str, 64, "%li", segid);
    status = pet_xml_add_val(region_xml, "segid", tmp_str);
    if (status != 0)
	goto out;

    snprintf(tmp_str, 64, "%lu", offset);
    status = pet_xml_add_val(region_xml, "offset", tmp_str);
    if (status != 0)
	goto out;

    return 0;

out:
    pet_xml_del_subtree(region_xml);
    return -1;
}


/*
static int
__hio_write_specification(hobbes_id_t enclave_id,
			  char      * fname,
			  pet_xml_t   xml)
{
    hcq_handle_t  hcq     = HCQ_INVALID_HANDLE;
    hobbes_file_t hfile   = HOBBES_INVALID_FILE;
    ssize_t       bytes   = 0;
    char        * xml_str = NULL;
    int           status  = 0;

    hcq = hobbes_open_enclave_cmdq(enclave_id);
    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not open enclave ocmmand queue\n");
	return -1;
    }

    hfile = hfio_open(hcq, fname, 
		O_WRONLY | O_CREAT,
		S_IRWXU | S_IRWXG | S_IROTH);
    if (hfile == HOBBES_INVALID_FILE) {
	ERROR("Could not open file on target enclave\n");
	goto out_hfile;
    }

    xml_str = pet_xml_get_str(xml);
    if (xml_str == NULL) {
	ERROR("Could not convert xml to string\n");
	goto out_str;
    }

    bytes = hfio_write(hfile, xml_str, strlen(xml_str));
    if ((size_t)bytes < strlen(xml_str)) {
	ERROR("Could not write xml (wrote %lu out of %li bytes\n",
		bytes, strlen(xml_str));
    }

out_str:
    hfio_close(hfile);

out_hfile:
    hobbes_close_enclave_cmdq(hcq);
    return status;

}
*/

static char *
hio_create_specification(char        * stub_name,
			 uint32_t      num_ranks,
			 uintptr_t     data_va,
			 uint64_t      data_size,
			 xemem_segid_t data_segid,
			 uintptr_t     heap_va,
			 uint64_t      heap_size,
			 xemem_segid_t heap_segid,
			 uintptr_t     stack_va,
			 uint64_t      stack_size,
			 xemem_segid_t stack_segid)
{
    pet_xml_t hio_xml     = PET_INVALID_XML;
    char      tmp_str[64] = {0};
    char    * xml_str     = NULL;
    int       status      = 0;

    hio_xml = pet_xml_new_tree("hio");
    if (hio_xml == PET_INVALID_XML) {
	ERROR("Could not create hio xml specification\n");
	return NULL;
    }

    /* Name */
    status = pet_xml_add_val(hio_xml, "name", stub_name);
    if (status != 0) {
	ERROR("Could not add name to xml specification\n");
	goto out;
    }

    /* Num ranks */
    snprintf(tmp_str, 64, "%u", num_ranks);
    status = pet_xml_add_val(hio_xml, "num_ranks", tmp_str);
    if (status != 0) {
	ERROR("Could not add num_ranks to xml specification\n");
	goto out;
    }

    /* Num regions */
    status = pet_xml_add_val(hio_xml, "num_regions", "3");
    if (status != 0) {
	ERROR("Could not add num_regions to xml specification\n");
	goto out;
    }

    status = __hio_prepare_region_specification(
		hio_xml,
		"data",
		data_va,
		data_size,
		data_segid,
		data_size);
    if (status != 0) {
	ERROR("Could not add data region to xml specification\n");
	goto out;
    }

    status = __hio_prepare_region_specification(
		hio_xml,
		"heap",
		heap_va,
		heap_size,
		heap_segid,
		heap_size);
    if (status != 0) {
	ERROR("Could not add heap region to xml specification\n");
	goto out;
    }

    status = __hio_prepare_region_specification(
		hio_xml,
		"stack",
		stack_va,
		stack_size,
		stack_segid,
		stack_size);
    if (status != 0) {
	ERROR("Could not add stack region to xml specification\n");
	goto out;
    }

    xml_str = pet_xml_get_str(hio_xml);
    pet_xml_free(hio_xml);

    return xml_str;

#if 0
    /* Write xml to target enclave */
    status = __hio_write_specification(
		enclave_id,
		fname,
		hio_xml);
    if (status != 0)
	ERROR("Could not write xml specification to target enclave\n");
#endif

out:
    pet_xml_free(hio_xml);
    return NULL;
}

hobbes_app_spec_t
hobbes_init_hio_app(hobbes_id_t hio_app_id,
		    char      * name,
		    char      * exe_path,
		    char      * hio_exe_path,
		    char      * hio_argv,
		    char      * hio_envp,
		    uint32_t    num_ranks,
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

    int status = 0;

    hobbes_app_spec_t spec = NULL;

    /* (1) VA base addresses and sizes */
    {
	/* Data va/size passed in as parameter now */

	/* Set heap base address */
	hio_set_heap_base_address(data_va, data_size, &heap_va);

	/* Set stack base address */
	hio_set_stack_base_address(stack_size, &stack_va);
    }

    /* (2) Export PA regions via XEMEM */
    {
	status = hio_export_region(
			data_pa,
			data_size * num_ranks,
			&data_local_va,
			&data_segid);
	if (status) {
	    ERROR("Could not export HIO data region\n");
	    goto out_data;
	}

	status = hio_export_region(
			heap_pa,
			heap_size * num_ranks,
			&heap_local_va,
			&heap_segid);
	if (status) {
	    ERROR("Could not export HIO heap region\n");
	    goto out_heap;
	}

	status = hio_export_region(
			stack_pa,
			stack_size * num_ranks,
			&stack_local_va,
			&stack_segid);
	if (status) {
	    ERROR("Could not export HIO stack region\n");
	    goto out_stack;
	}
    }

    /* (3) Create xml spec, add to argv, and create app spec */
    {
	char * xml_spec = NULL;
	char * stub_argv = NULL;

	/* Create xml specification */
	xml_spec = hio_create_specification(	
		name,
		num_ranks,
		data_va,
		data_size,
		data_segid,
		heap_va,
		heap_size,
		heap_segid,
		stack_va,
		stack_size,
		stack_segid);

	if (status != 0) {
	    ERROR("Could not write xml specification\n");
	    goto out_xml;
	}

	/* Prepend ARGV with spec */
	asprintf(&stub_argv, "'%s'", xml_spec);
	if (hio_argv)
	    asprintf(&stub_argv, "%s %s", stub_argv, hio_argv);

	/* Create app spec */
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

	free(xml_spec);
	free(stub_argv);

	if (!spec) {
	    ERROR("Could not prepare HIO app specification\n");
	    goto out_spec;
	}
    }

    printf("[app_launch] HIO region spec for stub:\n"
	"[app_launch] \tText and Data:\n"
	"[app_launch] \t\tVA: [0x%lx, 0x%lx)\n"
	"[app_launch] \t\tXEMEM segid: %li\n"
	"[app_launch] \tHeap:\n"
	"[app_launch] \t\tVA: [0x%lx, 0x%lx)\n"
	"[app_launch] \t\tXEMEM segid: %li\n"
	"[app_launch] \tStack:\n"
	"[app_launch] \t\tVA: [0x%lx, 0x%lx)\n"
	"[app_launch] \t\tXEMEM segid: %li\n",
	data_va, (data_va + data_size),
	data_segid,
	heap_va, (heap_va + heap_size),
	heap_segid,
	stack_va, (stack_va + stack_size),
	stack_segid);

    /* Save region info */
    hio_num_ranks          = num_ranks;

    hio_data.base_addr_va  = data_va;
    hio_data.size          = data_size;
    hio_data.local_va      = data_local_va;
    hio_data.segid         = data_segid;

    hio_heap.base_addr_va  = heap_va;
    hio_heap.size          = heap_size;
    hio_heap.local_va      = heap_local_va;
    hio_heap.segid         = heap_segid;

    hio_stack.base_addr_va = stack_va;
    hio_stack.size         = stack_size;
    hio_stack.local_va     = stack_local_va;
    hio_stack.segid        = stack_segid;

    return spec;

out_spec:
out_xml:
    xemem_remove(stack_segid);
    hio_unmap_region(stack_local_va, stack_size * num_ranks);

out_stack:
    xemem_remove(heap_segid);
    hio_unmap_region(heap_local_va, heap_size * num_ranks);

out_heap:
    xemem_remove(data_segid);
    hio_unmap_region(data_local_va, data_size * num_ranks);

out_data:
    return NULL;
}

int
hobbes_deinit_hio_app(void)
{
    xemem_remove(hio_stack.segid);
    hio_unmap_region(hio_stack.local_va, hio_stack.size * hio_num_ranks);

    xemem_remove(hio_heap.segid);
    hio_unmap_region(hio_heap.local_va, hio_heap.size * hio_num_ranks);

    xemem_remove(hio_data.segid);
    hio_unmap_region(hio_data.local_va, hio_data.size * hio_num_ranks);

    return 0;
}
