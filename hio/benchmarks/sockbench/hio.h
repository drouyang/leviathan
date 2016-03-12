#ifndef _HIO_H_
#define _HIO_H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/time.h>

extern int hio_status;
extern struct syscall_ops_t syscall_ops;

struct syscall_ops_t {
	int (*close)(int fd);
	int (*socket)(int socket_family, int socket_type, int protocol);
	int (*bind)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	int (*listen)(int sockfd, int backlog);
	int (*accept)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	ssize_t (*read)(int fd, void *buf, size_t count);
	ssize_t (*write)(int fd, const void *buf, size_t count);
	int (*fcntl3)(int fd, int cmd, int val);
	int (*epoll_create1)(int flags);
	int (*epoll_ctl)(int epfd, int op, int fd, struct epoll_event *event);
	int (*epoll_wait)(int epfd, struct epoll_event *events,
			int maxevents, int timeout);
	int (*getsockopt)(int sockfd, int level, int optname,
			void *optval, socklen_t *optlen);
	int (*setsockopt)(int sockfd, int level, int optname,
			const void *optval, socklen_t optlen);
	int (*select)(int nfds, fd_set *readfds, fd_set *writefds, 
			fd_set *exceptfds, struct timeval *timeout);
};

int hio_init(void);

#endif


