#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <fcntl.h>

#include <libhio.h>

#define PAGE_SIZE 4096

/* Use this format if each rank specifies its own HIO calls */
/*             fn name,   cmd,        ret, params ...       */
LIBHIO_CLIENT2(hio_open,  __NR_open,  int, const char *, int);
LIBHIO_CLIENT1(hio_close, __NR_close, int, int);
LIBHIO_CLIENT3(hio_read,  __NR_read,  ssize_t, int, void *, size_t);
LIBHIO_CLIENT3(hio_write, __NR_write, ssize_t, int, const void *, size_t);

/* Use this format if a single process will issue HIO calls for all ranks in the app
 * You must specify rank number in the 1st argument to the call
 */
/*                 fn name,      cmd,       rank_no,  ret, params ...        */
//LIBHIO_CLIENT2_APP(hio_open_app, __NR_open, uint32_t, int, const char *, int);

int 
main(int     argc,
     char ** argv) 
{
    char * pmi_rank = getenv("PMI_RANK");
    char * hio_name = getenv("STUB_NAME");
    int    rank = -1;
    int    status;

    if (argc != 2) {
        printf("Usage: %s <file to open>\n", *argv);
        return -1;
    }

    if (pmi_rank == NULL) {
        printf("No PMI_RANK in env. assuming rank 0\n");
        rank = 0;
    } else {
        rank = atoi(pmi_rank);
    }

    if (hio_name == NULL) {
        printf("No STUB_NAME in env. exiting\n");
        return -1;
    }

    sleep(2);

    status = libhio_client_init(hio_name, rank);
    if (status != 0) { 
        printf("Failed to init HIO client\n");
        return -1;
    }

    /* Open/read/write/close a file */
    {
        char * file_name = argv[1];
        int fd;
        ssize_t bytes;
        off_t offset;
        void * buf[PAGE_SIZE * 10];

        fd = hio_open(file_name, O_RDONLY);
        if (fd < 0) { 
            printf("Could not open file %s (fd=%d)\n", file_name, fd);
            libhio_client_deinit();
            return -1;
        }

        bytes = hio_read(fd, buf, PAGE_SIZE * 10);
        printf("Read %li bytes from fd %d\n", bytes, fd);
        if (bytes> 0)
            printf("%s\n", buf);

        hio_close(fd);

        fd = hio_open(file_name, O_WRONLY | O_TRUNC);
        if (fd < 0)  {
            printf("Could not open file %s (fd=%d)\n", file_name, fd);
            libhio_client_deinit();
            return -1;
        }

        bytes = hio_write(fd, "Overwrite file\n", 15);
        printf("Wrote %li bytes to fd %d\n", bytes, fd);

        hio_close(fd);
    }

    libhio_client_deinit();

    return 0;
} 
