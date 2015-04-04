
#include <stdlib.h>
#include <string.h>
#include <xpmem.h>

#include <pet_log.h>

#include "xemem.h"
#include "hobbes_db.h"

extern hdb_db_t hobbes_master_db;



xemem_segid_t
xemem_make(void          * vaddr, 
	   size_t          size,
	   int             permit_type,
	   void          * permit_value, 
	   char          * name)
{
    xemem_segid_t segid = 0;

    segid = xpmem_make(vaddr, size, permit_type, permit_value);
    
    if (segid <= 0) {
	ERROR("Could not create xemem segment\n");
	return segid;
    }

    hdb_create_xemem_segment(hobbes_master_db, segid, name);

    return segid;
}


xemem_segid_t
xemem_make_signalled(void   * vaddr, 
		    size_t   size,
		    int      permit_type,
		    void   * permit_value, 
		    char   * name, 
		    int    * fd)
{
    xemem_segid_t segid = 0;
    int tmp_fd = 0;

    if (fd == NULL) {
	ERROR("Invalid file descriptor pointer\n");
	return (xemem_segid_t)-1;
    }

    segid = xpmem_make_ext(vaddr, size, permit_type, permit_value, XPMEM_MEM_MODE | XPMEM_SIG_MODE, 0, &tmp_fd);
    
    if (segid <= 0) {
	ERROR("Could not create xemem segment\n");
	return segid;
    }

    hdb_create_xemem_segment(hobbes_master_db, segid, name);

    *fd = tmp_fd;
    return segid;
}

int
xemem_make_segid(void          * vaddr, 
		 size_t          size,
		 int             permit_type,
		 void          * permit_value, 
		 char          * name,
		 xemem_segid_t   segid)
{
    xemem_segid_t tmp_segid = 0;

    tmp_segid = xpmem_make_ext(vaddr, size, permit_type, permit_value, XPMEM_REQUEST_MODE, segid, NULL);
    
    if (tmp_segid != segid) {
	ERROR("Could not create xemem segment for segid (%ld)\n", segid);
	return -1;
    }

    hdb_create_xemem_segment(hobbes_master_db, segid, name);

    return 0;

}

int 
xemem_remove(xemem_segid_t segid)
{
    int ret = 0;
 
    ret = hdb_delete_xemem_segment(hobbes_master_db, segid);
    
    if (ret != 0) {
	ERROR("Could not delete segid (%ld) from database\n", segid);
	return -1;
    }

    ret = xpmem_remove(segid);

    if (ret != 0) {
	ERROR("Error removing segid (%ld) from XPMEM service\n", segid);
	return -1;
    }

    return 0;
}

xemem_apid_t 
xemem_get(xemem_segid_t segid, 
	  int           flags,
	  int           permit_type,
	  void        * permit_value)
{
    xemem_apid_t apid = 0;

    /* Do we need to do any DB updates? */

    apid = xpmem_get(segid, flags, permit_type, permit_value);

    return apid;
}

int 
xemem_release(xemem_apid_t apid)
{
    int ret = 0;

    /* Do we need to do any DB updates? */

    ret = xpmem_release(apid);

    return ret;
}

int
xemem_signal(xemem_apid_t apid)
{
    int ret = 0;

    /* Do we need to do any DB updates? */

    ret = xpmem_signal(apid);

    return ret;
}

int 
xemem_ack(int fd)
{
    int ret = 0;

    /* Do we need to do any DB updates? */

    ret = xpmem_ack(fd);

    return ret;
}



void *
xemem_attach(struct xemem_addr   addr, 
	     size_t              size,
	     void              * vaddr)
{
    void * new_vaddr = NULL;
    
    new_vaddr = xpmem_attach(*(struct xpmem_addr *)&addr, size, vaddr);

    /* TODO: Update attachment table */

    return new_vaddr;
} 

int 
xemem_detach(void * vaddr)
{
    int ret = 0;
    
    ret = xpmem_detach(vaddr);

    /* TODO: Update attachment table */

    return ret;

}


xemem_segid_t
xemem_lookup_segid(char * name)
{
    return hdb_get_xemem_segid(hobbes_master_db, name);
}

struct xemem_segment *
xemem_get_segment_list(int * num_segments)
{
    struct xemem_segment * seg_arr = NULL;
    xemem_segid_t        * id_arr  = NULL;

    int num_segs = 0;
    int i        = 0;


    id_arr = hdb_get_segments(hobbes_master_db, &num_segs);
    
    if (id_arr == NULL) {
	ERROR("Could not retrieve segment list from database\n");
	return NULL;
    }

    seg_arr = calloc(sizeof(struct xemem_segment), num_segs);

    for (i = 0; i < num_segs; i++) {
	seg_arr[i].segid = id_arr[i];

	strncpy(seg_arr[i].name, 
		hdb_get_xemem_name(hobbes_master_db, id_arr[i]), 
		XEMEM_SEG_NAME_LEN - 1);
    }

    free(id_arr);

    *num_segments = num_segs;
    return seg_arr;
}


int
xemem_export_segment(xemem_segid_t  segid,
		     char         * name)
{
    return hdb_create_xemem_segment(hobbes_master_db, segid, name);
}


int
xemem_remove_segment(xemem_segid_t segid)
{
    return hdb_delete_xemem_segment(hobbes_master_db, segid);
}
