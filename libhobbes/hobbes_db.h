/* Hobbes Database interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __HOBBES_DB_H__
#define __HOBBES_DB_H__

#include <stdint.h>

#include "hobbes_enclave.h"
#include "hobbes.h"
#include "hobbes_app.h"
#include "xemem.h"

struct hobbes_segment;

/* set master db size to 64MB for now */
#define HDB_MASTER_DB_SIZE  (64 * 1024 * 1024) 
#define HDB_MASTER_DB_SEGID (1)


typedef void * hdb_db_t;


typedef void * hdb_enclave_t;
typedef void * hdb_app_t;
typedef void * hdb_segment_t;
typedef void * hdb_pmi_keyval_t;


hdb_db_t hdb_create(uint64_t size);
hdb_db_t hdb_attach(void * db_addr);
void hdb_detach(hdb_db_t db);

int hdb_init_master_db(hdb_db_t db);


void * hdb_get_db_addr(hdb_db_t db);




/* 
 *  Creating/deleting enclave records
 */

 hobbes_id_t    hdb_create_enclave(hdb_db_t       db, 
				   char         * name, 
				   int            mgmt_dev_id, 
				   enclave_type_t type, 
				   hobbes_id_t    parent);

int             hdb_delete_enclave(hdb_db_t    db,
				   hobbes_id_t enclave_id);


hobbes_id_t *   hdb_get_enclaves(hdb_db_t   db, 
				 int      * num_enclaves);

/*
 * Enclave field Accessors 
 */
int             hdb_get_enclave_dev_id(hdb_db_t    db,
				       hobbes_id_t enclave_id);

int             hdb_set_enclave_dev_id(hdb_db_t    db,
				       hobbes_id_t enclave_id, 
				       int         dev_id);

hobbes_id_t     hdb_get_enclave_parent(hdb_db_t    db, 
				       hobbes_id_t enclave_id);

enclave_type_t  hdb_get_enclave_type(hdb_db_t    db, 
				     hobbes_id_t enclave_id);


enclave_state_t hdb_get_enclave_state(hdb_db_t    db,
				      hobbes_id_t enclave_id);

int             hdb_set_enclave_state(hdb_db_t        db,
				      hobbes_id_t     enclave_id, 
				      enclave_state_t state);


xemem_segid_t   hdb_get_enclave_cmdq(hdb_db_t    db,
				     hobbes_id_t enclave_id);

int             hdb_set_enclave_cmdq(hdb_db_t      db,
				     hobbes_id_t   enclave_id, 
				     xemem_segid_t segid);


char *          hdb_get_enclave_name(hdb_db_t    db, 
				     hobbes_id_t enclave_id);

hobbes_id_t     hdb_get_enclave_id(hdb_db_t   db, 
				   char     * enclave_name);



/* 
 *  XEMEM segments
 */

int             hdb_create_xemem_segment(hdb_db_t        db,
					 xemem_segid_t   segid,
					 char          * name,
					 hobbes_id_t     enclave_id,
					 hobbes_id_t     app_id);

int             hdb_delete_xemem_segment(hdb_db_t      db,
					 xemem_segid_t segid);


xemem_segid_t   hdb_get_xemem_segid(hdb_db_t   db, 
				    char     * name);


char *          hdb_get_xemem_name(hdb_db_t      db, 
				   xemem_segid_t segid);


hobbes_id_t     hdb_get_xemem_enclave(hdb_db_t      db,
				      xemem_segid_t segid);

hobbes_id_t     hdb_get_xemem_app(hdb_db_t      db,
				  xemem_segid_t segid);


xemem_segid_t * hdb_get_segments(hdb_db_t   db, 
				 int      * num_segments);



/* 
 * Applications
 */

hobbes_id_t     hdb_create_app(hdb_db_t     db, 
			       char       * name,
			       hobbes_id_t  enclave_id);
				   


int             hdb_delete_app(hdb_db_t    db,
			       hobbes_id_t app_id);

hobbes_id_t *   hdb_get_apps(hdb_db_t   db,
			     int      * num_apps);


/* Application field accessors */
hobbes_id_t     hdb_get_app_enclave(hdb_db_t      db,
				    hobbes_id_t   app_id);

app_state_t hdb_get_app_state(hdb_db_t        db,
			      hobbes_id_t     app_id);

int             hdb_set_app_state(hdb_db_t        db,
				  hobbes_id_t     app_id,
				  app_state_t state);

char *          hdb_get_app_name(hdb_db_t    db,
				 hobbes_id_t app_id);

hobbes_id_t     hdb_get_app_id(hdb_db_t db,
			       char *   app_name);


/*
 * PMI Key Value Store
 */

int hdb_put_pmi_keyval(hdb_db_t      db,
		       int           appid,
		       const char *  kvsname,
		       const char *  key,
		       const char *  val);

int hdb_get_pmi_keyval(hdb_db_t      db,
		       int           appid,
		       const char *  kvsname,
		       const char *  key,
		       const char ** val);

#endif
