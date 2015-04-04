/* Pisces init task interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <stdint.h>

#include <pet_log.h>

#include "pisces.h"
#include "pisces_cmds.h"


/* Pisces device file IOCTLs */
#define PISCES_STAT_FILE   500
#define PISCES_LOAD_FILE   501
#define PISCES_WRITE_FILE  502


/* IOCTL argument data structures */
struct pisces_user_file_info {
    unsigned long long user_addr;
    unsigned int       path_len;
    char               path[0];
} __attribute__((packed)); 



#define PISCES_CTRL_PATH "/dev/pisces"



static int pisces_fd = 0;

int 
pisces_init( void )
{
    pisces_fd = open(PISCES_CTRL_PATH, O_RDWR);
    
    if (pisces_fd < 0) {
	ERROR("Error opening pisces cmd file (%s)\n", PISCES_CTRL_PATH);
	return -1;
    }
    
    pisces_cmd_init();
    
    return 0;
}


int 
pisces_get_fd( void )
{
    return pisces_fd;
}



size_t
pisces_file_stat(char * filename)
{
    struct pisces_user_file_info * file_info = NULL;

    int    path_len  = strlen(filename) + 1;
    size_t file_size = 0;
    
    file_info = calloc(1, sizeof(struct pisces_user_file_info) + path_len);

    file_info->path_len = path_len;
    strncpy(file_info->path, filename, path_len - 1);

    file_size = ioctl(pisces_fd, PISCES_STAT_FILE, file_info);

    free(file_info);
    return file_size;
}



/** 
 * Load a file into memory from the management enclave
 * Assumes that enough memory has already been allocated
 */
int 
pisces_file_load(char * filename,
		 void * addr)
{
    struct pisces_user_file_info * file_info = NULL;

    int path_len = strlen(filename) + 1;
    int ret      = 0;
    
    file_info = calloc(1, sizeof(struct pisces_user_file_info) + path_len);

    file_info->user_addr = (uintptr_t)addr;
    file_info->path_len  = path_len;
    strncpy(file_info->path, filename, path_len - 1);

    ret = ioctl(pisces_fd, PISCES_LOAD_FILE, file_info);

    free(file_info);

    return ret;
}
