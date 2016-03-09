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
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include <libhio.h>
#include <xemem.h>

#define PAGE_SIZE sysconf(_SC_PAGESIZE)


static int
hio_open(const char * pathname,
         int          flags)
{
    return open(pathname, flags);
}
LIBHIO_STUB2(hio_open, int, const char *, int);

static int
hio_close(int fd)
{
    return close(fd);
}
LIBHIO_STUB1(hio_close, int, int);

static ssize_t 
hio_read(int    fd,
         void * buf,
         size_t count)
{
    return read(fd, buf, count);
}
LIBHIO_STUB3(hio_read, ssize_t, int, void *, size_t);

static ssize_t
hio_write(int          fd,
          const void * buf,
          size_t       count)
{
    return write(fd, buf, count);
}
LIBHIO_STUB3(hio_write, ssize_t, int, const void *, size_t);

static void *
hio_mmap(void          * addr,
         size_t          length,
         int             prot,
         int             flags,
         int             fd,
         off_t           offset,
         hio_segment_t * seg)
{
    void * result;
    xemem_segid_t segid;

    result = mmap(addr, length, prot, flags, fd, offset);
    if (result == MAP_FAILED) 
        return result;

    /* Export the mapping */
    segid = xemem_make(result, length, NULL);
    if (segid == XEMEM_INVALID_SEGID) {
        munmap(result, length);
        return MAP_FAILED;
    }

    /* Set the seg parameters */
    seg->segid     = segid;
    seg->size      = length;
    seg->page_size = PAGE_SIZE;
    seg->vaddr     = result;

    return result;
}
LIBHIO_STUB7(hio_mmap, void *, void *, size_t, int, int, int, off_t, hio_segment_t *);

static int
hio_munmap(void          * addr,
           size_t          length,
           hio_segment_t * seg)
{
    xemem_remove(seg->segid);
    return munmap(addr, length);
}
LIBHIO_STUB3(hio_munmap, int, void *, size_t, hio_segment_t *);

static int
hio_ioctl(int              fd,
          int              request,
          char           * argp,
          hio_segment_t ** seg_list,
          uint32_t       * nr_segs)
{
    return ioctl(fd, request, argp);
}
LIBHIO_STUB5(hio_ioctl, int, int, int, char *, hio_segment_t ** , uint32_t *);

static int 
hio_socket(int 	domain, 
           int 	type, 
           int 	protocol)
{
	return socket(domain, type, protocol);

}
LIBHIO_STUB3(hio_socket, int, int, int, int);

static int 
hio_bind(int 			  sockfd, 
     const struct sockaddr 	* addr,
     socklen_t 			  addrlen)
{
	return bind(sockfd, addr, addrlen);
}
LIBHIO_STUB3(hio_bind, int, int, const struct sockaddr *, socklen_t);


static int 
hio_listen(int 		sockfd, 
	   int 		backlog)
{
	return listen(sockfd, backlog);
}
LIBHIO_STUB2(hio_listen, int, int, int);


static int 
hio_accept(int 			  sockfd, 
	   struct sockaddr  	* addr, 
	   socklen_t 		* addrlen) 
{
	return accept(sockfd, addr, addrlen);

}
LIBHIO_STUB3(hio_accept, int, int, struct sockaddr *, socklen_t *);


static int 
hio_connect(int 			   sockfd, 
	    const struct sockaddr 	 * addr,
	    socklen_t	 		   addrlen)
{
	return connect(sockfd, addr, addrlen);

}
LIBHIO_STUB3(hio_connect,int, int, const struct sockaddr *, socklen_t);

static int 
hio_getsockopt(int 		sockfd, 
               int              level, 
               int              optname, 
               void            *optval,
               socklen_t       *optlen) {
	return getsockopt(sockfd, level, optname, optval, optlen);
}
LIBHIO_STUB5(hio_getsockopt, int, int, int, int, void *, socklen_t *);

static int hio_setsockopt(int		sockfd, 
                          int           level, 
                          int           optname,
                          const void   *optval, 
                          socklen_t     optlen) {
	return setsockopt(sockfd, level, optname, optval, optlen);
}
LIBHIO_STUB5(hio_setsockopt, int, int, int, int, void *, socklen_t);

static int hio_fcntl3(int fd, int cmd, int val) {
	return fcntl(fd, cmd, val);
}
LIBHIO_STUB3(hio_fcntl3, int, int, int, int);

static int hio_epoll_create1(int flags) {
	return epoll_create1(flags);
}
LIBHIO_STUB1(hio_epoll_create1, int, int);

static int hio_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	return epoll_ctl(epfd, op, fd, event);
}
LIBHIO_STUB4(hio_epoll_ctl, int, int, int, int, struct epoll_event *);

static int hio_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
	return epoll_wait(epfd, events, maxevents, timeout);
}
LIBHIO_STUB4(hio_epoll_wait, int, int, struct epoll_event *, int, int);

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

    status = libhio_register_stub_fn(__NR_socket, hio_socket);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_bind, hio_bind);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_listen, hio_listen);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_accept, hio_accept);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_connect, hio_connect);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_getsockopt, hio_getsockopt);
    if (status)
        return -1;

    status = libhio_register_stub_fn(__NR_setsockopt, hio_setsockopt);
    if (status) return -1;

    status = libhio_register_stub_fn(__NR_fcntl, hio_fcntl3);
    if (status) return -1;

    status = libhio_register_stub_fn(__NR_epoll_create1, hio_epoll_create1);
    if (status) return -1;

    status = libhio_register_stub_fn(__NR_epoll_ctl, hio_epoll_ctl);
    if (status) return -1;

    status = libhio_register_stub_fn(__NR_epoll_wait, hio_epoll_wait);
    if (status) return -1;

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
