/* Hobbes client program interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#include "client.h"

#include <sys/mman.h>
#include <errno.h>
#include <string.h>


hdb_db_t hobbes_master_db = NULL;
static xpmem_apid_t hobbes_db_apid;

int 
hobbes_client_init()
{
    void * db_addr = NULL;

    hobbes_db_apid = xpmem_get(HDB_MASTER_DB_SEGID, XPMEM_RDWR, XPMEM_PERMIT_MODE, NULL);

    if (hobbes_db_apid <= 0) {
        printf("xpmem get failed\n");
        return -1;
    }


    {
	struct xpmem_addr addr;

        addr.apid   = hobbes_db_apid;
        addr.offset = 0;

        db_addr = xpmem_attach(addr, HDB_MASTER_DB_SIZE, NULL);
        if (db_addr == MAP_FAILED) {
            printf("xpmem attach failed\n");
            xpmem_release(hobbes_db_apid);
            return -1;
        }
    }

    printf("Attaching to local database (db_addr=%p)\n", db_addr);

    hobbes_master_db  = hdb_attach(db_addr);

    if (hobbes_master_db == NULL) {
	printf("Error: Could not connect to database\n");
	return -1;
    }


    return 0;
}



int
hobbes_client_deinit()
{
    void * db_addr = NULL;

    db_addr = hdb_get_db_addr(hobbes_master_db);
    
    hdb_detach(hobbes_master_db);

    xpmem_detach(db_addr);

    xpmem_release(hobbes_db_apid);

    return 0;
}


int
hobbes_client_export_segment(xpmem_segid_t segid,
                             char        * name)
{
    if (strlen(name) >= HOBBES_MAX_SEGMENT_NAME_LEN)
        return -EINVAL;

    return hdb_export_segment(hobbes_master_db, segid, name);
}


int
hobbes_client_remove_segment(xpmem_segid_t segid)
{
    return hdb_remove_segment(hobbes_master_db, segid);
}


xpmem_segid_t
hobbes_client_get_segid_by_name(char * name)
{
    struct hobbes_segment segment;

    if (strlen(name) >= HOBBES_MAX_SEGMENT_NAME_LEN)
        return -EINVAL;

    if (hdb_get_segment_by_name(hobbes_master_db, name, &segment) != 0)
        return -ENOENT;

    return segment.segid;
}

struct hobbes_segment *
hobbes_client_get_segment_list(int * num_segments)
{
    if (num_segments == NULL)
        return NULL;

    return hdb_get_segment_list(hobbes_master_db,  num_segments);
}
