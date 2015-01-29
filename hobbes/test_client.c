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
