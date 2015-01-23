#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include <dbapi.h>
#include <dballoc.h>

#include <xpmem.h>

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

#define DB_SIZE  (PAGE_SIZE * 100)

#undef XPMEM_DEV_PATH
#define XPMEM_DEV_PATH "/xpmem"

int main(int argc, char ** argv) {
    xpmem_segid_t segid;
    xpmem_apid_t  apid;

    void * db_addr = NULL;
    void * db      = NULL;
    
    segid = strtoll(argv[1], NULL, 10);

    printf("%lli\n", segid);


    apid = xpmem_get(segid, XPMEM_RDWR, XPMEM_PERMIT_MODE, NULL);

    printf("apid: %lli\n", apid);

    if (apid <= 0) {
        printf("xpmem get failed\n");
        return -1;
    }


    {
	struct xpmem_addr addr;

        addr.apid = apid;
        addr.offset = 0;

        db_addr = xpmem_attach(addr, DB_SIZE, NULL);
        if (db_addr == MAP_FAILED) {
            printf("xpmem attach failed\n");
            xpmem_release(apid);
            return -1;
        }
    }

    printf("Attaching to local database (db_addr=%p)\n", db_addr);

    db = wg_attach_existing_local_database(db_addr);
    

    if (db == NULL) {
	printf("Error: Could not connect to database\n");
	return -1;
    }


    printf("DB attached\n");

    {
	char * str = NULL;
	void * rec = wg_find_record_str(db, 0, WG_COND_EQUAL, "Hello DB", NULL);

	str = wg_decode_str(db, wg_get_field(db, rec, 0));
	printf("str=%s\n", str);
	
    }


    wg_detach_local_database(db);


    xpmem_detach(db_addr);
    xpmem_release(apid);

    return 0;

}
