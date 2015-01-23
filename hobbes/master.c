#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dbapi.h>
#include <dballoc.h>

#include <xpmem.h>

#include "hobbes_db.h"

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

#define DB_SIZE  (PAGE_SIZE * 100)


void * 
create_master_db(unsigned int size) 
{
    void * db_addr = NULL;
    void * db      = NULL;
    
    xpmem_segid_t segid;

    if (size % PAGE_SIZE) {
	printf("Error: Database must be integral number of pages\n");
	return NULL;
    }

    db = wg_attach_local_database(size);

    if (db == NULL) {
	printf("Could not create database\n");
	return NULL;
    }


    /* Initialize Master DB State */
    {
	void * rec = NULL;

	rec = wg_create_record(db, 2);
	wg_set_field(db, rec, 0, wg_encode_int(db, HDB_NEXT_ENCLAVE));
	wg_set_field(db, rec, 1, wg_encode_int(db, 0));

	rec = wg_create_record(db, 2);
	wg_set_field(db, rec, 0, wg_encode_int(db, HDB_NEXT_PROCESS));
	wg_set_field(db, rec, 1, wg_encode_int(db, 0));


	/* Create MAster enclave */

	/* Create Master Process */
	
    }

#ifdef USE_DATABASE_HANDLE
    db_addr = ((db_handle *)db)->db;
#else
    db_addr = db;
#endif

    segid = xpmem_make(db_addr, size, XPMEM_PERMIT_MODE, (void *)0600);


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

    db = create_master_db(DB_SIZE);

    void * rec = wg_create_record(db, 10);
    wg_int enc = wg_encode_str(db, "Hello DB", NULL);

    wg_set_field(db, rec, 0, enc);

    

    while (1) {sleep(1);}


}
