/*
** selectserver.c -- a cheezy multiperson chat server
* Modified from http://beej.us/guide/bgnet/examples/selectserver.c
* Modified by Jiannan Ouyang <ouyang@cs.pitt.edu>, 03/12/2016
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "hio.h"
#define PORTNUM 3369   // port we're listening on
extern struct syscall_ops_t syscall_ops;
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
	int yes=1;        // for setsockopt() SO_REUSEADDR, below

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons( PORTNUM );

	if ((fd = syscall_ops.socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("error: socket");
		return -1;
	}

	// lose the pesky "address already in use" error message
	syscall_ops.setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	if (syscall_ops.bind(fd, 
		(const struct sockaddr *)&serverAddr, 
		sizeof(serverAddr)) < 0) {
		printf("error: bind");
		return -1;
	}

	return fd;
}

int main(void)
{
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int s;
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[256];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int i, j, rv;


#ifdef LWK
    if (hio_init() < 0) {
	    perror ("hio_init");
	    abort ();
    }
    printf("Init hio operations\n");
#endif

    listener = create_and_bind();
    if (listener == -1) abort();

    // listen
    s = syscall_ops.listen(listener, SOMAXCONN);
    if (s == -1) {
	    perror ("listen");
	    abort ();
    }
    printf("Listening at socket %d port %d\n", listener, PORTNUM);

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (syscall_ops.select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
		    newfd = syscall_ops.accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

		    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
			printf("selectserver: new connection from %s on " "socket %d\n",
				inet_ntop(remoteaddr.ss_family, 
					&(((struct sockaddr_in*)&remoteaddr)->sin_addr),
					remoteIP, 
					INET6_ADDRSTRLEN),
				newfd);
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = syscall_ops.read(i, buf, sizeof buf)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
		        // echo
			if (syscall_ops.write(i, buf, nbytes) == -1) {
			    perror("echo");
			}
#if 0
                        // we got some data from a client
                        for(j = 0; j <= fdmax; j++) {
                            // send to everyone!
                            if (FD_ISSET(j, &master)) {
                                // except the listener and ourselves
                                if (j != listener && j != i) {
                                    if (syscall_ops.write(j, buf, nbytes) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        }
#endif
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}

