/*
 * From: https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
 * Modified by Jiannan Ouyang <ouyang@cs.pitt.edu>, 03/08/2016
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#include "hio.h"
extern struct syscall_ops_t syscall_ops;

#define MAXEVENTS 64
#define PORTNUM 3369
struct sockaddr_in serverAddr;

static int make_socket_non_blocking (int sfd)
{
  int flags, s;

  flags = syscall_ops.fcntl3(sfd, F_GETFL, 0);
  if (flags == -1) {
      perror ("fcntl");
      return -1;
  }

  flags |= O_NONBLOCK;
  s = syscall_ops.fcntl3(sfd, F_SETFL, flags);
  if (s == -1) {
      perror ("fcntl");
      return -1;
  }

  return 0;
}

static int create_and_bind (void) {
	int fd;

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons( PORTNUM );

	if ((fd = syscall_ops.socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("error: socket");
		return -1;
	}

	if (syscall_ops.bind(fd, 
		(const struct sockaddr *)&serverAddr, 
		sizeof(serverAddr)) < 0) {
		printf("error: bind");
		return -1;
	}

	return fd;
}

int main (int argc, char *argv[]) {
  int sfd, s;
  int efd;
  struct epoll_event event;
  //struct epoll_event *events;
  struct epoll_event events[MAXEVENTS];

#ifdef LWK
  if (hio_init() < 0) {
      perror ("hio_init");
      abort ();
  }
  printf("Init hio operations\n");
#endif

  sfd = create_and_bind();
  if (sfd == -1) abort();
  printf("Listening at socket %d port %d\n", sfd, PORTNUM);

  s = make_socket_non_blocking (sfd);
  if (s == -1) abort ();
  printf("Non-blocking socket is set\n");

  s = syscall_ops.listen (sfd, SOMAXCONN);
  if (s == -1) {
      perror ("listen");
      abort ();
  }

  efd = syscall_ops.epoll_create1 (0);
  if (efd == -1) {
      perror ("epoll_create");
      abort ();
  }

  event.data.fd = sfd;
  event.events = EPOLLIN | EPOLLET;
  s = syscall_ops.epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
  if (s == -1)
    {
      perror ("epoll_ctl");
      abort ();
    }

  /* Buffer where events are returned */
  //events = calloc (MAXEVENTS, sizeof event);

  /* The event loop */
  while (1) {
      int n, i;

      n = syscall_ops.epoll_wait (efd, events, MAXEVENTS, -1);
      for (i = 0; i < n; i++) {
	  if ((events[i].events & EPOLLERR) ||
              (events[i].events & EPOLLHUP) ||
              (!(events[i].events & EPOLLIN))) {
              /* An error has occured on this fd, or the socket is not
                 ready for reading (why were we notified then?) */
	      printf ("epoll error\n");
	      syscall_ops.close (events[i].data.fd);
	      continue;
	  } else if (sfd == events[i].data.fd) {
              /* We have a notification on the listening socket, which
                 means one or more incoming connections. */
              while (1) {
                  struct sockaddr in_addr;
                  socklen_t in_len;
                  int infd;
                  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                  in_len = sizeof in_addr;
                  infd = syscall_ops.accept (sfd, &in_addr, &in_len);
                  if (infd == -1) {
                      if ((errno == EAGAIN) ||
                          (errno == EWOULDBLOCK)) {
                          /* We have processed all incoming
                             connections. */
                          break;
		      } else {
			  printf("accept error %d\n", sfd);
                          perror ("accept");
                          break;
		      }
		  }

                  /* Make the incoming socket non-blocking and add it to the
                     list of fds to monitor. */
                  s = make_socket_non_blocking (infd);
                  if (s == -1)
                    abort ();

                  event.data.fd = infd;
                  event.events = EPOLLIN | EPOLLET;
                  s = syscall_ops.epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event);
                  if (s == -1) {
                      perror ("epoll_ctl");
                      abort ();
		  }
	      }
	      continue;
	  } else {
              /* We have data on the fd waiting to be read. Read and
                 display it. We must read whatever data is available
                 completely, as we are running in edge-triggered mode
                 and won't get a notification again for the same
                 data. */
              int done = 0;

              while (1) {
                  ssize_t count;
                  char buf[512];

		  printf("read at %d, 0x%p, %d\n", events[i].data.fd, buf, sizeof buf);
                  count = syscall_ops.read (events[i].data.fd, buf, sizeof buf);
                  if (count == -1) {
                      /* If errno == EAGAIN, that means we have read all
                         data. So go back to the main loop. */
                      if (errno != EAGAIN) {
                          perror ("read");
                          done = 1;
		      }
		      break;
		  } else if (count == 0) {
                      /* End of file. The remote has closed the
                         connection. */
                      done = 1;
                      break;
		  }

                  /* Write the buffer to standard output */
                  s = syscall_ops.write (1, buf, count);
                  if (s == -1) {
                      perror ("write");
                      abort ();
		  }
	      }

	      if (done) {
                  printf ("Closed connection on descriptor %d\n",
                          events[i].data.fd);

                  /* Closing the descriptor will make epoll remove it
                     from the set of descriptors which are monitored. */
		  close (events[i].data.fd);
	      }
	  }
      }
  }

  //free (events);
  syscall_ops.close (sfd);

  return EXIT_SUCCESS;
}
