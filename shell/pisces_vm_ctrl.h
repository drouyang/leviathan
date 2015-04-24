/* Pisces VM control 
 *  (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __PISCES_VM_CTRL_H__
#define __PISCES_VM_CTRL_H__

#include <pet_xml.h>
#include <hobbes.h>


int  create_pisces_vm(pet_xml_t xml, char * name);
int destroy_pisces_vm(hobbes_id_t enclave_id);



#endif
