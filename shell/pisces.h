/* Hobbes Pisces enclave functions
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __PISCES_ENCLAVE_CTRL_H__
#define __PISCES_ENCLAVE_CTRL_H__


#include <pet_xml.h>

#include "hobbes.h"

int pisces_enclave_create(pet_xml_t xml, char * name);
int pisces_enclave_destroy(hobbes_id_t enclave_id);
int pisces_enclave_console(hobbes_id_t enclave_id);


#endif
