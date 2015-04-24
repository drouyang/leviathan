/* Linux VM control functions 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __LINUX_VM_CTRL_H__
#define __LINUX_VM_CTRL_H__

#include <pet_xml.h>
#include <hobbes.h>

int  create_linux_vm(pet_xml_t xml, char * name);
int destroy_linux_vm(hobbes_id_t enclave_id);

#endif
