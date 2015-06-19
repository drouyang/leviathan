/* Hobbes Local File I/O handlers 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stddef.h>

#include <pet_xml.h>
#include <pet_log.h>

#include <hobbes_cmd_queue.h>
#include <hobbes_util.h>

#include "file_io.h"


#define MAX_XFER_SIZE (4096)

struct hfio_wr_req {
    uint64_t file_handle;
    uint64_t data_size;
    uint8_t  data[0];
} __attribute__((packed));

struct hfio_rd_req {
    uint64_t file_handle;
    uint64_t data_size;
} __attribute__((packed));


struct hfio_seek_req {
    uint64_t file_handle;
    uint64_t offset;
    uint32_t whence;
} __attribute__((packed));


int
file_stat_handler(hcq_handle_t hcq, 
		  hcq_cmd_t    cmd)
{
    char     * path     = NULL;
    uint32_t   path_len = 0;

    struct stat stat_buf;

    int ret = 0;

    memset(&stat_buf, 0, sizeof(struct stat));

    path = hcq_get_cmd_data(hcq, cmd, &path_len);

    if (path == NULL) {
	ERROR("Could not read stat command string\n");
	goto err;
    }

    ret = stat(path, &stat_buf);

    if (ret != 0) {
	ERROR("Error stating local file (%s)\n", path);
	goto err;
    }

    
    hcq_cmd_return(hcq, cmd, 0, sizeof(struct stat), (void *)&stat_buf);
    return 0;

 err:
    hcq_cmd_return(hcq, cmd, -1, 0, NULL);
    return 0;
}

int 
file_fstat_handler(hcq_handle_t hcq, 
		   hcq_cmd_t    cmd)
{
    uint64_t * cmd_data = NULL;
    uint32_t   cmd_size = 0;
    
    struct stat stat_buf;
    
    int ret = 0;
    int fd  = 0;

    memset(&stat_buf, 0, sizeof(struct stat));

    cmd_data = hcq_get_cmd_data(hcq, cmd, &cmd_size);

    if ( (cmd_data == NULL) || (cmd_size != sizeof(uint64_t)) ) {
	ERROR("Could not read stat command\n");
	goto err;
    }

    fd = *(int *)cmd_data;

    ret = fstat(fd, &stat_buf);

    if (ret != 0) {
	ERROR("Error stating local file\n");
	goto err;
    }

    
    hcq_cmd_return(hcq, cmd, 0, sizeof(struct stat), (void *)&stat_buf);
    return 0;

 err:
    hcq_cmd_return(hcq, cmd, -1, 0, NULL);
    return 0;
}


int
file_open_handler(hcq_handle_t hcq, 
		  hcq_cmd_t    cmd)
{
    uint64_t   file_handle = 0;
    uint32_t   data_size   = 0;
    pet_xml_t  cmd_xml     = PET_INVALID_XML;
    char     * xml_str     = NULL;

    char * path  = NULL;
    int    flags = 0;
    int    mode  = 0;

    xml_str = hcq_get_cmd_data(hcq, cmd, &data_size);

    if (xml_str == NULL) {
	ERROR("Could not read Open command string\n");
	goto out;
    }

    cmd_xml = pet_xml_parse_str(xml_str);
    
    if (cmd_xml == PET_INVALID_XML) {
	ERROR("Invalid XML syntax in File open handler\n");
	goto out;
    }

    
    path  = pet_xml_get_val(cmd_xml, "path");
    flags = smart_atoi(-1, pet_xml_get_val(cmd_xml, "flags"));

    if (flags == -1) {
	ERROR("Invalid file flags in file open command handler\n");
	goto out;
    }

    /* Value only matters if O_CREATE is set */
    mode = smart_atoi(0, pet_xml_get_val(cmd_xml, "mode"));

    file_handle  = open(path, flags, mode);

    pet_xml_free(cmd_xml);
    hcq_cmd_return(hcq, cmd, 0, sizeof(uint64_t), &file_handle);

    return 0;

 out:
    if (cmd_xml != PET_INVALID_XML)  pet_xml_free(cmd_xml);
    hcq_cmd_return(hcq, cmd, -1, 0, NULL);

    return 0;
}



int 
file_close_handler(hcq_handle_t hcq,
		   hcq_cmd_t    cmd)
{
    uint32_t   data_size   = 0;
    uint64_t   file_handle = 0;
    void     * data_ptr    = NULL;

    data_ptr    = hcq_get_cmd_data(hcq, cmd, &data_size);
    file_handle = *(uint64_t *)data_ptr;

    close((int)file_handle);

    hcq_cmd_return(hcq, cmd, 0, 0, NULL);

    return 0;
}


int
file_read_handler(hcq_handle_t hcq,
		  hcq_cmd_t    cmd)
{
    struct hfio_rd_req * req = NULL;
    uint32_t req_size = 0;
    uint8_t  * dst_buf = NULL;
    ssize_t total_read = 0;

    int fd  =  0;
    int ret = -1;

    req = hcq_get_cmd_data(hcq, cmd, &req_size);

    if (req_size != sizeof(struct hfio_rd_req)) {
	ERROR("Invalid read request format\n");
	goto out;
    }
    
    if (req->data_size > MAX_XFER_SIZE) {
	req->data_size = MAX_XFER_SIZE;
    }

    fd = req->file_handle;

    dst_buf = calloc(req->data_size, 1);

    {
	ssize_t left_to_read = req->data_size;

	while (left_to_read > 0) {
	    int bytes_read = read(fd, dst_buf + total_read, left_to_read);

	    if (bytes_read <= 0) {
		break;
	    }
	    
	    total_read   += bytes_read;
	    left_to_read -= bytes_read;
	}
    }
    
    ret = total_read;

 out:
    hcq_cmd_return(hcq, cmd, ret, total_read, dst_buf);

    if (dst_buf != NULL) free(dst_buf);
    return 0;
}

int 
file_write_handler(hcq_handle_t hcq,
		   hcq_cmd_t    cmd)
{
    struct hfio_wr_req * req = NULL;
    uint32_t req_size        = 0;
    ssize_t  total_wrote     = 0;

    int fd  =  0;
    int ret =  0;

    req = hcq_get_cmd_data(hcq, cmd, &req_size);

    if ( (req == NULL) || (req_size < sizeof(struct hfio_wr_req)) ) {
	ERROR("Invalid Write request format\n");
	goto out;
    }

    if (req->data_size > MAX_XFER_SIZE) {
	req->data_size = MAX_XFER_SIZE;
    }

    fd = req->file_handle;

    {
	ssize_t left_to_write = req->data_size;

	while (left_to_write > 0) {
	    int bytes_wrote = write(fd, req->data + total_wrote, left_to_write);

	    printf("wrote %d bytes\n", bytes_wrote);

	    if (bytes_wrote <= 0) {
		break;
	    }

	    total_wrote   += bytes_wrote;
	    left_to_write -= bytes_wrote;
	}
    }

    ret = total_wrote;
    
 out:
    hcq_cmd_return(hcq, cmd, ret, 0, NULL);
    return 0;
}


int 
file_seek_handler(hcq_handle_t hcq,
		  hcq_cmd_t    cmd)
{
    struct hfio_seek_req * req = NULL;
    uint32_t req_size          = 0;
    
    int fd  =  0;
    int ret = -1;
    off_t ret_val = 0;


    req = hcq_get_cmd_data(hcq, cmd, &req_size);

    if ( (req == NULL) || (req_size < sizeof(struct hfio_seek_req)) ) {
	ERROR("Invalid seek request format\n");
	goto out;
    }

    fd      = req->file_handle;

    ret_val = lseek(fd, req->offset, req->whence);

    ret     = 0;
    
 out:
    hcq_cmd_return(hcq, cmd, ret, sizeof(off_t), (void *)&ret_val); 

    return 0;
}
