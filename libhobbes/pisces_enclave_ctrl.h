/* Hobbes Pisces enclave functions
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __ENCLAVE_PISCES_H__
#define __ENCLAVE_PISCES_H__


#include <pet_xml.h>

#include "hobbes.h"

int pisces_enclave_create(pet_xml_t xml, char * name);
int pisces_enclave_destroy(hobbes_id_t enclave_id);


#endif
