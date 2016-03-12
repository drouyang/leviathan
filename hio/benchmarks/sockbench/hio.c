#include "hio.h"

#include <sys/syscall.h>
#include <lwk/aspace.h>
#include <lwk/pmem.h>
#include <xemem.h>
#include <libhio.h>

/* Use this format if each rank specifies its own HIO calls */
/*             fn name,   cmd,        ret, params ...       */
LIBHIO_CLIENT2(hio_open,   __NR_open,   int, const char *, int);
LIBHIO_CLIENT1(hio_close,  __NR_close,  int, int);
LIBHIO_CLIENT3(hio_read,   __NR_read,   ssize_t, int, void *, size_t);
LIBHIO_CLIENT3(hio_write,  __NR_write,  ssize_t, int, const void *, size_t);
LIBHIO_CLIENT7(hio_mmap,   __NR_mmap,   void *, void *, size_t, int, int, int, off_t, hio_segment_t *);
LIBHIO_CLIENT3(hio_munmap, __NR_munmap, int, void *, size_t, hio_segment_t *);

LIBHIO_CLIENT3(hio_socket, __NR_socket, int, int, int, int);
LIBHIO_CLIENT3(hio_bind,   __NR_bind, int, int, const struct sockaddr *, socklen_t);
LIBHIO_CLIENT2(hio_listen, __NR_listen,   int, int, int);
LIBHIO_CLIENT3(hio_accept, __NR_accept, int, int, struct sockaddr *, socklen_t *);
LIBHIO_CLIENT3(hio_connect,__NR_connect, int, int, const struct sockaddr *, socklen_t);
LIBHIO_CLIENT3(hio_fcntl3, __NR_fcntl, int, int, int, int);
LIBHIO_CLIENT1(hio_epoll_create1,  __NR_epoll_create1,  int, int);
LIBHIO_CLIENT4(hio_epoll_ctl, __NR_epoll_ctl, int, int, int, int, struct epoll_event *);
LIBHIO_CLIENT4(hio_epoll_wait, __NR_epoll_wait, int, int, struct epoll_event *, int, int);
LIBHIO_CLIENT5(hio_getsockopt, __NR_getsockopt, int, int, int, int, void *, socklen_t *);
LIBHIO_CLIENT5(hio_setsockopt, __NR_setsockopt, int, int, int, int, const void *, socklen_t);
LIBHIO_CLIENT5(hio_select, __NR_select, int, int, fd_set *, fd_set *, fd_set *, struct timeval *);

struct syscall_ops_t syscall_ops = {
	.close = close,
	.socket = socket,
	.bind = bind,
	.listen = listen,
	.accept = accept,
	.read = read,
	.write = write,
	.fcntl3 = (int (*)(int, int, int))fcntl,
	.epoll_create1 = epoll_create1,
	.epoll_ctl = epoll_ctl,
	.epoll_wait = epoll_wait,
	.getsockopt = getsockopt,
	.setsockopt = setsockopt,
	.select = select
};

int hio_init(void) {
	char * pmi_rank = getenv("PMI_RANK");
	char * hio_name = getenv("STUB_NAME");
	int    rank = -1;
	int    status;

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

	syscall_ops.close = hio_close;
	syscall_ops.socket = hio_socket;
	syscall_ops.bind = hio_bind;
	syscall_ops.listen = hio_listen;
	syscall_ops.accept = hio_accept;
	syscall_ops.read = hio_read;
	syscall_ops.write = hio_write;
	syscall_ops.fcntl3 = hio_fcntl3;
	syscall_ops.epoll_create1 = hio_epoll_create1;
	syscall_ops.epoll_ctl = hio_epoll_ctl;
	syscall_ops.epoll_wait = hio_epoll_wait;
	syscall_ops.getsockopt = hio_getsockopt;
	syscall_ops.setsockopt = hio_setsockopt;
	syscall_ops.select = hio_select;
}
