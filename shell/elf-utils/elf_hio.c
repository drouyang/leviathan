/* elfls: Copyright (C) 1999,2011 by Brian Raiter <breadbox@muppetlabs.com>
 * License GPLv2+: GNU GPL version 2 or later.
 * This is free software; you are free to change and redistribute it.
 * There is NO WARRANTY, to the extent permitted by law.
 *
 * 2016: Modified by Brian Kocoloski to integrate with Hobbes shell
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <elf.h>

#include "elfrw.h"
#include "../hio.h"

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif

/* Memory allocation error message.
 */
#define nomem() (fputs("Out of memory!\n", stderr), exit(EXIT_FAILURE))

/* Structure used to organize strings to be displayed in a list.
 */
typedef	struct textline {
    char       *str;	/* the string to display */
    int		size;	/* the current length of the string */
    int		left;	/* how much more the string can grow */
} textline;

/* The global variables.
 */
static Elf64_Ehdr	elffhdr;	/* ELF header of current file */
static Elf64_Phdr      *proghdr = NULL;	/* program header table */
static Elf64_Shdr      *secthdr = NULL;	/* section header table */
static char	       *sectstr = NULL;	/* sh string table */

static int		proghdrs;	/* FALSE if no ph table */
static int		secthdrs;	/* FALSE if no sh table */
static Elf64_Phdr      *phentry = NULL;	/* ph with the entry point */
static Elf64_Shdr      *shshstr = NULL;	/* sh with the sh string table */

static char const      *thefilename;	/* name of current file */
static FILE	       *thefile;	/* handle to current file */

/* The error-reporting function.
 */
static int err(char const *fmt, ...)
{
    va_list args;

    if (fmt) {
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
    } else {
	fprintf(stderr, "%s: %s", thefilename, strerror(errno));
    }
    fputc('\n', stderr);
    return 0;
}


/*
 * Generic file-reading functions.
 */

/* Read a piece of the current file into a freshly allocated buffer.
 */
static void *getarea(long offset, unsigned long size)
{
    void       *buf;

    if (fseek(thefile, offset, SEEK_SET))
	return NULL;
    if (!(buf = malloc(size)))
	nomem();
    if (fread(buf, size, 1, thefile) != 1) {
	free(buf);
	return NULL;
    }
    return buf;
}

/*
 * Functions for examining ELF structures.
 */

/* Verify that the given ELF identifier is appropriate to our
 * expectations.
 */
static int checkelfident(unsigned char const id[EI_NIDENT])
{
    if (memcmp(id, ELFMAG, SELFMAG))
	return err("%s: not an ELF file.", thefilename);
    if (id[EI_CLASS] != ELFCLASS32 && id[EI_CLASS] != ELFCLASS64)
	return err("%s: unrecognized ELF class: %d.",
		   thefilename, (int)id[EI_CLASS]);
    if (id[EI_DATA] != ELFDATA2MSB && id[EI_DATA] != ELFDATA2LSB)
	return err("%s: unrecognized ELF data: %d.",
		   thefilename, id[EI_DATA]);
    if (id[EI_VERSION] != EV_CURRENT)
	return err("%s: unrecognized ELF version: %d.",
		   thefilename, (int)id[EI_VERSION]);

    return TRUE;
}

/* Read in the ELF header proper, and verify that its contents conform
 * to what the program can decipher.
 */
static int readelfhdr(void)
{
    if (elfrw_read_Ehdr(thefile, &elffhdr) != 1) {
	if (ferror(thefile))
	    return err(NULL);
	else
	    return err("%s: not an ELF file.", thefilename);
    }
    if (!checkelfident(elffhdr.e_ident))
	return FALSE;

    switch (elffhdr.e_type) {
      case ET_REL:
      case ET_EXEC:
      case ET_DYN:
      case ET_CORE:
	break;
      default:
	return err("%s: unknown ELF file type (type = %u).",
		   thefilename, elffhdr.e_type);
    }
    if (elffhdr.e_ehsize != sizeof(Elf32_Ehdr) &&
			elffhdr.e_ehsize != sizeof(Elf64_Ehdr))
	return err("%s: warning: unrecognized ELF header size: %d.",
		   thefilename, elffhdr.e_ehsize);
    if (elffhdr.e_version != EV_CURRENT)
	return err("%s: unrecognized ELF header version: %u.",
		   thefilename, (unsigned int)elffhdr.e_version);

    if (elffhdr.e_phoff != 0) {
	if (elffhdr.e_phentsize != sizeof(Elf32_Phdr) &&
			elffhdr.e_phentsize != sizeof(Elf64_Phdr))
	    err("%s: unrecognized program header entry size: %u.",
		thefilename, elffhdr.e_phentsize);
	else
	    proghdrs = 1;
    }
    if (elffhdr.e_shoff != 0) {
	if (elffhdr.e_shentsize != sizeof(Elf32_Shdr) &&
			elffhdr.e_shentsize != sizeof(Elf64_Shdr))
	    err("%s: unrecognized section header entry size: %u.",
		thefilename, elffhdr.e_shentsize);
	else
	    secthdrs = 1;
    }
    return TRUE;
}

/* Read in the program header table, if it is present. If the ELF
 * header lists an entry point, then compare the entry point with the
 * load address of the program header entries to determine which one
 * contains said point.
 */
static int readproghdrs(void)
{
    int i, n;

    if (!proghdrs)
	return TRUE;
    n = elffhdr.e_phnum;
    if (!(proghdr = malloc(n * sizeof *proghdr)))
	nomem();
    if (fseek(thefile, elffhdr.e_phoff, SEEK_SET) ||
			elfrw_read_Phdrs(thefile, proghdr, n) != n) {
	err("%s: invalid program header table offset.", thefilename);
	proghdrs = FALSE;
	return TRUE;
    }
    if (elffhdr.e_entry) {
	for (i = 0 ; i < n ; ++i) {
	    if (proghdr[i].p_type == PT_LOAD
			&& elffhdr.e_entry >= proghdr[i].p_vaddr
			&& elffhdr.e_entry < proghdr[i].p_vaddr
						 + proghdr[i].p_memsz) {
		phentry = proghdr + i;
		break;
	    }
	}
    }

    return TRUE;
}

/* Read in the section header table, if it is present. If a section
 * header string table is given in the ELF header, then also load the
 * string table contents.
 */
static int readsecthdrs(void)
{
    int n;

    if (!secthdrs)
	return TRUE;
    n = elffhdr.e_shnum;
    if (!(secthdr = malloc(n * sizeof *secthdr)))
	nomem();
    if (fseek(thefile, elffhdr.e_shoff, SEEK_SET) ||
			elfrw_read_Shdrs(thefile, secthdr, n) != n) {
	err("%s: warning: invalid section header table offset.", thefilename);
	secthdrs = FALSE;
	return TRUE;
    }
    if (elffhdr.e_shstrndx != SHN_UNDEF) {
	shshstr = secthdr + elffhdr.e_shstrndx;
	if (!(sectstr = getarea(shshstr->sh_offset, shshstr->sh_size))) {
	    err("%s: warning: invalid string table location.", thefilename);
	    free(sectstr);
	    sectstr = NULL;
	}
    }

    return TRUE;
}

static int
get_binary_data_address_and_size(uint64_t    page_size,
				 uintptr_t * address_p,
				 uint64_t  * size_p)
{
    uintptr_t min_address = UINTPTR_MAX;
    uintptr_t max_address = 0;
    int i, n;

    *address_p = 0;
    *size_p    = 0;

    if (!proghdrs)
	return 0;

    /* Loop through the program header table, finding the minimum and maximum virtual
     * addresses for binary data. PT_LOAD entries are sorted by p_vaddr
     */
    n = elffhdr.e_phnum;
    for (i = 0; i < n; i++) {

	Elf64_Phdr * p_entry  = &(proghdr[i]);

	if (p_entry->p_type == PT_LOAD) {
	    uintptr_t start_addr = PAGE_ALIGN_DOWN(p_entry->p_vaddr, page_size);
	    uintptr_t end_addr   = PAGE_ALIGN_UP(p_entry->p_vaddr + p_entry->p_memsz, page_size);

	    if (start_addr < min_address)
		min_address = start_addr;

	    max_address = end_addr;
	}
    }

    *address_p = min_address;
    *size_p    = max_address - min_address;

    return TRUE;
}





/*
 * Top-level functions.
 */
int
hobbes_parse_elf_binary_data(char      * exe_path,
  			     uint64_t    page_size,
   		             uintptr_t * base_addr,
		             uint64_t  * size)
{
    thefilename = exe_path;
    *base_addr  = 0;
    *size       = 0;

    if (!(thefile = fopen(thefilename, "rb"))) {
	err("Cannot open file %s: %s", thefilename, strerror(errno));
	return -1;
    }

    if (!readelfhdr() || !readproghdrs() || !readsecthdrs()) {
	err("Cannot read ELF {program/section} headers\n");
	fclose(thefile);
	return -1;
    }

    if (!get_binary_data_address_and_size(page_size, base_addr, size)) {
	err("Cannot parse ELF program headers\n");
	fclose(thefile);
	return -1;
    }

    fclose(thefile);
    free(proghdr);
    free(secthdr);
    free(sectstr);

    return 0;
}
