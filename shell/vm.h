/*  VM control 
 *  (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __VM_H__
#define __VM_H__

#include <pet_xml.h>
#include <hobbes.h>

int  create_vm_main(int argc, char ** argv);
int destroy_vm_main(int argc, char ** argv);


int hobbes_create_vm(char      * cfg_file, 
		     char      * name,
		     hobbes_id_t host_enclave_id);


int hobbes_destroy_vm(hobbes_id_t enclave_id);

#endif
