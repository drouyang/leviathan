/*  HIO support
 *  (c) 2016, Brian Kocoloski <briankoco@cs.pitt.edu>
 */

#ifndef __HIO_H__
#define __HIO_H__

#include <hobbes.h>
#include <hobbes_app.h>

#include <pet_xml.h>

hobbes_app_spec_t
hobbes_init_hio_app(
	hobbes_id_t   app_id,
	char	    * name,
	char  	    * exe_path,
	char 	    * hio_exe_path,
	char	    * hio_argv,
	char	    * hio_envp,
	int	      use_lage_pages,
	uint64_t      heap_size,
	uint64_t      stack_size,
	uint32_t      numa_node);


int
hobbes_deinit_hio_app(void);

/* Defined in elf-utils/elf_hio.c */
int
hio_parse_elf_binary_data(char      * exe_path,
			  uintptr_t * base_addr,
			  uint64_t  * size);

#endif
