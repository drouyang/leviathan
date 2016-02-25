#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <libhio.h>
#include <xemem.h>


static int
hio_open(const char     * pathname,
         int              flags,
         hio_segment_t ** seg_list,
         uint32_t       * nr_segs)
{
    return open(pathname, flags);
}

static int
hio_close(int              fd,
          hio_segment_t ** seg_list,
          uint32_t       * nr_segs)
{
    return close(fd);
}

static ssize_t 
hio_read(int              fd,
         void           * buf,
         size_t           count,
         hio_segment_t ** seg_list,
         uint32_t       * nr_segs)
{
    return read(fd, buf, count);
}

static ssize_t
hio_write(int              fd,
          const void     * buf,
          size_t           count,
          hio_segment_t ** seg_list,
          uint32_t       * nr_segs)
{
    return write(fd, buf, count);
}

static void *
hio_mmap(void           * addr,
         size_t           length,
         int              prot,
         int              flags,
         int              fd,
         off_t            offset,
         hio_segment_t ** seg_list,
         uint32_t       * nr_segs)
{
    void * result;
    xemem_segid_t segid;

    result = mmap(addr, length, prot, flags, fd, offset);
    if (result == MAP_FAILED)
        return result;

    /* Export the mapping */
    segid = xemem_make(addr, length, NULL);
    if (segid == XEMEM_INVALID_SEGID) {
        munmap(addr, length);
        return MAP_FAILED;
    }

    /* Add the segment to the hio seg list */
    {
        hio_segment_t * seg = malloc(sizeof(hio_segment_t));
        if (seg == NULL) {
            xemem_remove(segid);
            munmap(addr, length);
            return MAP_FAILED;
        }

        seg->segid = segid;
        seg->size  = length;
        seg->vaddr = result;

        *seg_list = seg;
        *nr_segs  = 1;
    }

    return result;
}

static int
hio_munmap(void           * addr,
           size_t           length,
           hio_segment_t ** seg_list,
           uint32_t       * nr_segs)
{
    return munmap(addr, length);
}

static int
hio_ioctl(int              fd,
          int              request,
          char           * argp,
          hio_segment_t ** seg_list,
          uint32_t       * nr_segs)
{
    return ioctl(fd, request, argp);
}

/* Define libhio handler functions */
LIBHIO_STUB2(hio_open, int, const char *, int);
LIBHIO_STUB1(hio_close, int, int);
LIBHIO_STUB3(hio_read, ssize_t, int, void *, size_t);
LIBHIO_STUB3(hio_write, ssize_t, int, const void *, size_t);
LIBHIO_STUB6(hio_mmap, void *, void *, size_t, int, int, int, off_t);
LIBHIO_STUB2(hio_munmap, int, void *, size_t);
LIBHIO_STUB3(hio_ioctl, int, int, int, char *);


static int
libhio_register_stub_fns(void)
{
    int status;

    status = libhio_register_stub_fn(__NR_open, hio_open);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_close, hio_close);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_read, hio_read);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_write, hio_write);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_mmap, hio_mmap);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_munmap, hio_munmap);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_ioctl, hio_ioctl);
    if (status)
        return -1;

    return 0;
}

int 
main(int     argc,
     char ** argv)
{
    int status = 0;

    status = libhio_stub_init(&argc, &argv);
    if (status != 0)
        return -1;

    status = libhio_register_stub_fns();
    if (status != 0) {
        fprintf(stderr, "Cannot register hio stubs\n");
        goto out;
    }

    /* Enter the event loop */
    status = libhio_event_loop();

out:
    libhio_stub_deinit();
    return status;
}
