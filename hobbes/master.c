#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dbapi.h>
#include <dballoc.h>

#include <xpmem.h>

#include "hobbes_db.h"

#define PAGE_SIZE sysconf(_SC_PAGESIZE)





void * 
create_master_db(unsigned int size) 
{

    xpmem_segid_t segid = HDB_MASTER_DB_SEGID;
    void *   db_addr    = NULL;
    hdb_db_t db         = hdb_create(size);
   
    /* Initialize Master DB State */
    {
	void * rec = NULL;
	struct hobbes_enclave enclave;

	rec = wg_create_record(db, 3);
	wg_set_field(db, rec, 0, wg_encode_int(db, HDB_ENCLAVE_HDR));
	wg_set_field(db, rec, 1, wg_encode_int(db, 0));
	wg_set_field(db, rec, 2, wg_encode_int(db, 0));

	rec = wg_create_record(db, 2);
	wg_set_field(db, rec, 0, wg_encode_int(db, HDB_NEXT_PROCESS));
	wg_set_field(db, rec, 1, wg_encode_int(db, 0));


	/* Create Master enclave */
	hdb_create_enclave(db, "master", 0, MASTER_ENCLAVE, 0);

	hdb_get_enclave_by_id(db, 0, &enclave);

	printf("Master Enclave id =%d\n", enclave.enclave_id);
	printf("Master enclave name=%s\n", enclave.name);

	enclave.state = ENCLAVE_RUNNING;
	
	hdb_update_enclave(db, &enclave);

	/* Create Master Process */
       
    }

    db_addr = hdb_get_db_addr(db);

    //segid = xpmem_make(db_addr, size, XPMEM_REQUEST_MODE, (void *)segid);
    segid = xpmem_make_hobbes(db_addr, size, XPMEM_PERMIT_MODE, (void *)0777,
        XPMEM_MEM_MODE | XPMEM_REQUEST_MODE, segid, NULL);


    if (segid <= 0) {
	printf("Error Creating SegID (%llu)\n", segid);
	wg_delete_local_database(db);
	return NULL;
    } 

    printf("segid: %llu\n", segid);

    return db;
}


int main(int argc, char ** argv) {
    
    void * db = NULL;

    db = create_master_db(HDB_MASTER_DB_SIZE);

    if (db == NULL) {
	printf("Error creating master database\n");
	return -1;
    }

    wg_print_db(db);

    while (1) {sleep(1);}

    return 0;
}
