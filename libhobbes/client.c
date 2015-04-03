/* Hobbes client program interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#include "client.h"

#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "xemem.h"

hdb_db_t hobbes_master_db = NULL;
static xemem_apid_t hobbes_db_apid;

int 
hobbes_client_init()
{
    void * db_addr = NULL;

    hobbes_db_apid = xemem_get(HDB_MASTER_DB_SEGID, XEMEM_RDWR, XEMEM_PERMIT_MODE, NULL);

    if (hobbes_db_apid <= 0) {
        printf("xpmem get failed\n");
        return -1;
    }


    {
	struct xemem_addr addr;

        addr.apid   = hobbes_db_apid;
        addr.offset = 0;

        db_addr = xemem_attach(addr, HDB_MASTER_DB_SIZE, NULL);
        if (db_addr == MAP_FAILED) {
            printf("xpmem attach failed\n");
            xemem_release(hobbes_db_apid);
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

    xemem_detach(db_addr);

    xemem_release(hobbes_db_apid);

    return 0;
}
