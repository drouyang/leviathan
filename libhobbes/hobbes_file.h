/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */



#ifndef __HOBBES_FILE_H__
#define __HOBBES_FILE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "hobbes_cmd_queue.h"

#define HOBBES_INVALID_FILE (NULL)

#define HFIO_MAX_XFER_SIZE (4096)

struct hobbes_file_state;
typedef struct hobbes_file_state * hobbes_file_t;

hobbes_file_t hfio_open(hcq_handle_t hcq, char * path, int flags, ...);

int     hfio_stat (hcq_handle_t  hcq,  char * path, struct stat * buf);
int     hfio_fstat(hobbes_file_t file, struct stat * buf);
ssize_t hfio_read (hobbes_file_t file, char * buf,  size_t count);
ssize_t hfio_write(hobbes_file_t file, const char * buf,  size_t count);
off_t   hfio_lseek(hobbes_file_t file, off_t offset, int whence);
void    hfio_close(hobbes_file_t file);


/* High level file transfer avoiding buffer size constraints */
ssize_t
hfio_read_file(hobbes_file_t file, char * buf, size_t count);

ssize_t
hfio_write_file(hobbes_file_t file, const char * buf, size_t count);

/* Copy a file from one enclave to another */
int 
hobbes_copy_file(char      * path,
		 hobbes_id_t src_enclave,
		 hobbes_id_t dst_enclave);


/*
  DIR *opendir(const char *name);
  int closedir(DIR *dirp);
  struct dirent *readdir(DIR *dirp);
  int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
*/


/* 
ssize_t hfio_pread(hobbes_file_t file, void * buf, size_t count, off_t offset);
ssize_t hfio_pwrite(hobbes_file_t file, const void * buf, size_t count, off_t offset);

   ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
   ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
   ssize_t preadv(int fd, const struct iovec *iov, int iovcnt,
            off_t offset);
   ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt,
            off_t offset);
*/

#ifdef __cplusplus
}
#endif

#endif
