/*  HIO ("Hobbes I/O") support
 *  (c) 2016, Brian Kocoloski <briankoco@cs.pitt.edu>
 */

#ifndef __HIO_H__
#define __HIO_H__

#include <hobbes.h>
#include <hobbes_app.h>

#include <pet_xml.h>


#define PAGE_SIZE_4KB			(1ULL << 12)
#define PAGE_SIZE_2MB			(1ULL << 21)

#define PAGE_MASK(ps)			(~(ps - 1))
#define PAGE_ALIGN_DOWN(addr, ps)	(addr & PAGE_MASK(ps))
#define PAGE_ALIGN_UP(addr, ps)		((addr + (ps - 1)) & PAGE_MASK(ps))

hobbes_app_spec_t
hobbes_init_hio_app(
	hobbes_id_t   app_id,
	char	    * name,
	char  	    * exe_path,
	char 	    * hio_exe_path,
	char	    * hio_argv,
	char	    * hio_envp,
	uint64_t      page_size,
	uintptr_t     data_va,
	uintptr_t     data_pa,
	uintptr_t     heap_pa,
	uintptr_t     stack_pa,
	uint64_t      data_size,
	uint64_t      heap_size,
	uint64_t      stack_size);


int
hobbes_deinit_hio_app(void);


/* Defined in elf-utils/elf_hio.c */
int
hobbes_parse_elf_binary_data(char      * exe_path,
			     uint64_t    page_size,
			     uintptr_t * data_base_va,
			     uint64_t  * data_size);

#endif
