/*
 *	T T C P . C
 *
 * Test TCP connection.  Makes a connection on port 5001
 * and transfers fabricated buffers or data copied from stdin.
 *
 * Usable on 4.2, 4.3, and 4.1a systems by defining one of
 * BSD42 BSD43 (BSD41a)
 * Machines using System V with BSD sockets should define SYSV.
 *
 * Modified for operation under 4.2BSD, 18 Dec 84
 *      T.C. Slattery, USNA
 * Minor improvements, Mike Muuss and Terry Slattery, 16-Oct-85.
 * Modified in 1989 at Silicon Graphics, Inc.
 *	catch SIGPIPE to be able to print stats when receiver has died 
 *	for tcp, don't look for sentinel during reads to allow small transfers
 *	increased default buffer size to 8K, nbuf to 2K to transfer 16MB
 *	moved default port to 5001, beyond IPPORT_USERRESERVED
 *	make sinkmode default because it is more popular, 
 *		-s now means don't sink/source 
 *	count number of read/write system calls to see effects of 
 *		blocking from full socket buffers
 *	for tcp, -D option turns off buffered writes (sets TCP_NODELAY sockopt)
 *	buffer alignment options, -A and -O
 *	print stats in a format that's a bit easier to use with grep & awk
 *	for SYSV, mimic BSD routines to use most of the existing timing code
 *
 * Distribution Status -
 *      Public Domain.  Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "ttcp.c $Revision: 1.2 $";
#endif

#define volatile 

/* #define BSD42 */
/* #define BSD41a */
#if defined(sgi)
#define SYSV
#endif

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/time.h>		/* struct timeval */

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
LIBHIO_CLIENT3(hio_accept, __NR_accept, int, int, const struct sockaddr *, socklen_t *);
LIBHIO_CLIENT3(hio_connect,__NR_connect, int, int, const struct sockaddr *, socklen_t);

extern int hio_status;

#if defined(SYSV)
#include <sys/times.h>
#include <sys/param.h>
#define RUSAGE_SELF 0
#else
#include <sys/resource.h>
#endif

struct sockaddr_in sinme;
struct sockaddr_in sinhim;
struct sockaddr_in frominet;

int domain, fromlen;
int fd;				/* fd of network socket */

unsigned long buflen = 2 * 1024 * 1024;	/* length of buffer */
char *buf;			/* ptr to dynamic buffer */
unsigned long nbuf = 2 * 1024;		/* number of buffers to send in sinkmode */

int bufoffset = 0;		/* align buffer to this */
int bufalign = 16*1024;		/* modulo this */

int udp = 0;			/* 0 = tcp, !0 = udp */
int options = 0;		/* socket options */
int so_rcvbuf = 0;		/* SO_RCVBUF socket option input buffer size */
int so_sndbuf = 0;		/* SO_SNDBUF socket option output buffer size */

int one = 1;                    /* for 4.3 BSD style setsockopt() */
short port = 5001;		/* TCP port number */
char *host;			/* ptr to name of host */
int trans;			/* 0=receive, !0=transmit mode */
int sinkmode = 1;		/* 0=normal I/O, !0=sink/source mode */
int verbose = 0;		/* 0=print basic info, 1=print cpu rate, proc
				 * resource usage. */
int nodelay = 0;		/* set TCP_NODELAY socket option */
int b_flag = 0;			/* use mread() */

struct hostent *addr;
extern int errno;

char Usage[] = "\
Usage: ttcp -t [-options] host [ < in ]\n\
       ttcp -r [-options > out]\n\
Common options:\n\
	-l##	length of bufs read from or written to network (default 8192)\n\
	-u	use UDP instead of TCP\n\
	-p##	port number to send to or listen at (default 5001)\n\
	-s	-t: don't source a pattern to network, get data from stdin\n\
		-r: don't sink (discard), print data on stdout\n\
	-A	align the start of buffers to this modulus (default 16384)\n\
	-O	start buffers at this offset from the modulus (default 0)\n\
	-v	verbose: print more statistics\n\
	-d	set SO_DEBUG socket option\n\
Options specific to -t:\n\
	-n##	number of source bufs written to network (default 2048)\n\
	-D	don't buffer TCP writes (sets TCP_NODELAY socket option)\n\
	-S##	set SO_SNDBUF socket option for output buffer size of\n\
	        ## bytes\n\
Options specific to -r:\n\
	-B	for -s, only output full blocks as specified by -l (for TAR)\n\
	-R##	set SO_RCVBUF socket option for input buffer size of ## bytes\n";

char stats[128];
unsigned long nbytes;		/* bytes on net */
unsigned long numCalls;		/* # of I/O system calls */

void prep_timer();
void  read_timer();
unsigned long delta_ms;

void
sigpipe()
{
}

main(argc,argv)
int argc;
char **argv;
{
	unsigned long addr_tmp;

#ifdef LWK
	//if (argc < 2) goto usage;
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
#endif

	argv++; argc--;
	while( argc>0 && argv[0][0] == '-' )  {
		switch (argv[0][1]) {

		case 'B':
			b_flag = 1;
			break;
		case 't':
			trans = 1;
			break;
		case 'r':
			trans = 0;
			break;
		case 'd':
			options |= SO_DEBUG;
			break;
		case 'D':
			nodelay = 1;
			break;
		case 'n':
			nbuf = atoi(&argv[0][2]);
			break;
		case 'l':
			buflen = atoi(&argv[0][2]);
			break;
		case 'R':
			/* socket opt for input buffer size */
			so_rcvbuf = atoi(&argv[0][2]);
			break;
		case 'S':
			/* socket opt for output buffer size*/
			so_sndbuf = atoi(&argv[0][2]);
			break;
		case 's':
			sinkmode = 0;	/* sink/source data */
			break;
		case 'p':
			port = atoi(&argv[0][2]);
			break;
		case 'u':
			udp = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'A':
			bufalign = atoi(&argv[0][2]);
			break;
		case 'O':
			bufoffset = atoi(&argv[0][2]);
			break;
		default:
			goto usage;
		}
		argv++; argc--;
	}
	if(trans)  {
		/* xmitr */
		if (argc != 1) goto usage;
		memset((char *)&sinhim, 0, sizeof(sinhim));
		host = argv[0];
		if (atoi(host) > 0 )  {
			/* Numeric */
			sinhim.sin_family = AF_INET;
#if defined(cray)
			addr_tmp = inet_addr(host);
			sinhim.sin_addr = addr_tmp;
#else
			sinhim.sin_addr.s_addr = inet_addr(host);
#endif
		} else {
			if ((addr=gethostbyname(host)) == NULL)
				err("bad hostname");
			sinhim.sin_family = addr->h_addrtype;
			memset((char *)&addr_tmp, addr->h_addr, addr->h_length);
#if defined(cray)
			sinhim.sin_addr = addr_tmp;
#else
			sinhim.sin_addr.s_addr = addr_tmp;
#endif cray
		}
		sinhim.sin_port = htons(port);
		sinme.sin_port = 0;		/* free choice */
	} else {
		/* rcvr */
		sinme.sin_port =  htons(port);
	}


	if (udp && buflen < 5) {
	    buflen = 5;		/* send more than the sentinel size */
	}

	if ( (buf = (char *)malloc(buflen+bufalign)) == (char *)NULL)
		err("malloc");
	if (bufalign != 0)
		buf +=(bufalign - ((int)buf % bufalign) + bufoffset) % bufalign;

	    fprintf(stdout,
	    "ttcp-t: buflen=%d KB, nbuf=%d, align=%d/+%d, port=%d  %s\n",
		buflen/1024, nbuf, bufalign, bufoffset, port,
		udp?"udp":"tcp");

#ifdef LWK
	if ((fd = hio_socket(AF_INET, udp?SOCK_DGRAM:SOCK_STREAM, 0)) < 0)
#else 
	if ((fd = socket(AF_INET, udp?SOCK_DGRAM:SOCK_STREAM, 0)) < 0)
#endif
		err("socket");
	mes("socket");
	printf("socket fd = %d\n", fd);

#ifdef LWK
	if (hio_bind(fd, (const struct sockaddr *)&sinme, sizeof(sinme)) < 0)
#else 
	if (bind(fd, (const struct sockaddr *)&sinme, sizeof(sinme)) < 0)
#endif
		err("bind");
	mes("bind");

	if (!udp)  {
	    //signal(SIGPIPE, sigpipe);
	    if (trans) {
		/* CLIENT */
		if (options || so_rcvbuf || so_sndbuf)  {
		        set_options(options);
		}
		if (nodelay) {
			struct protoent *p;
			p = getprotobyname("tcp");
			if( p && setsockopt(fd, p->p_proto, TCP_NODELAY, 
			    &one, sizeof(one)) < 0)
				err("setsockopt: nodelay");
			mes("nodelay");
		}

#ifdef LWK
		if(hio_connect(fd, (const struct sockaddr *)&sinhim, sizeof(sinhim) ) < 0)
#else
		if(connect(fd, (const struct sockaddr *)&sinhim, sizeof(sinhim) ) < 0)
#endif
			err("connect");
		mes("connect");
	    } else {
		/* SERVER */
#ifdef LWK
		hio_listen(fd,0);   /* allow a queue of 0 */
#else
		listen(fd,0);   /* allow a queue of 0 */
#endif
		mes("listen");
#if 0
/* Solaris fix */
		listen(fd, 1);   /* allow a queue of 0 */
#endif

		if (options || so_sndbuf || so_rcvbuf) {
			set_options (options);
		}

		fromlen = sizeof(frominet);
		domain = AF_INET;

#ifdef LWK
		if((fd=hio_accept(fd, (const struct sockaddr *)&frominet, &fromlen) ) < 0)
#else
		if((fd=accept(fd, (const struct sockaddr *)&frominet, &fromlen) ) < 0)
#endif
			err("accept");
		mes("accept");
		printf("accept fd = %d\n", fd);
/*
		{ struct sockaddr_in peer;
		  int peerlen = sizeof(peer);
		  if (getpeername(fd, (struct sockaddr_in *) &peer, 
				&peerlen) < 0) {
			err("getpeername");
		  }
		  if (options || so_sndbuf || so_rcvbuf) {
			set_options (options);
		  }
		  fprintf(stderr,"ttcp-r: accept from %s\n", 
			inet_ntoa(peer.sin_addr));
		}
*/
	    }
	}
	prep_timer();
	errno = 0;

	{      
		register int cnt;
		if (trans)  {
			// Client
			pattern( buf, buflen );
			if(udp)  (void)Nwrite( fd, buf, 4 ); /* rcvr start */
			while (nbuf-- && Nwrite(fd,buf,buflen) == buflen)
				nbytes += buflen;
			if(udp)  (void)delay( 100000 );
			if(udp)  (void)Nwrite( fd, buf, 4 ); /* rcvr end */
		} else {
			// Server
			if (udp) {
			    while ((cnt=Nread(fd,buf,buflen)) > 0)  {
				    static int going = 0;
				    if( cnt <= 4 )  {
					    if( going )
						    break;	/* "EOF" */
					    going = 1;
					    prep_timer();
				    } else {
					    nbytes += cnt;
				    }
			    }
			} else {
			    while ((cnt=Nread(fd,buf,buflen)) > 0)  {
				    nbytes += cnt;
			    }
			}
		}
	} 

	if(errno) err("IO");
	read_timer();
	if(udp&&trans)  {
		(void)Nwrite( fd, buf, 4 ); /* rcvr end */
		(void)Nwrite( fd, buf, 4 ); /* rcvr end */
		(void)Nwrite( fd, buf, 4 ); /* rcvr end */
		(void)Nwrite( fd, buf, 4 ); /* rcvr end */
	}
	fprintf(stdout, "ttcp%s: %ld MB (%ld KB) in %lld ms\n",
		trans?"-t":"-r",
		nbytes/1024/1024, nbytes/1024, delta_ms);
	fprintf(stdout, "ttcp%s: Throughput: %f MB/sec\n",
		trans?"-t":"-r",
		(nbytes/1024.0/1024.0)/(delta_ms/1024.0));
	exit(0);

usage:
	fprintf(stderr,Usage);
	exit(1);
}

void err(char *s)
{
	fprintf(stderr,"ttcp%s: ", trans?"-t":"-r");
	perror(s);
	fprintf(stderr,"errno=%d\n",errno);
	exit(1);
}

void mes(char *s)
{
	fprintf(stderr,"ttcp%s: %s\n", trans?"-t":"-r", s);
}

void pattern(char *cp, int cnt)
{
	register char c;
	c = 0;
	while( cnt-- > 0 )  {
		while( !isprint((c&0x7F)) )  c++;
		*cp++ = (c++&0x7F);
	}
}


static struct	timeval start;	/* Time at which timing started */

static void tvsub();

void prep_timer() {
	gettimeofday(&start, (struct timezone *)0);
}

void read_timer() {
	struct timeval now;
	struct timeval tdiff;

	gettimeofday(&now, NULL);
	tvsub(&tdiff, &now, &start);
	delta_ms = tdiff.tv_sec * 1000 + tdiff.tv_usec/1024;
}

/*
 *			set_options
 */
int
set_options(options)
int options;
{
#if defined(BSD42)
	if (setsockopt(fd, SOL_SOCKET, options, 0, 0) < 0)
		err("setsockopt");
#else BSD43
        if (options & SO_DEBUG) {
	  if (setsockopt(fd, SOL_SOCKET, SO_DEBUG, &one,
		       sizeof(one)) < 0)
		err("setsockopt SO_DEBUG");
	}

        if (so_sndbuf) {
	  if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &so_sndbuf,
			 sizeof(so_sndbuf)) < 0)
		err("setsockopt SO_SNDBUF");
	}

        if (so_rcvbuf) {
	  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &so_rcvbuf,
			 sizeof(so_rcvbuf)) < 0)
		err("setsockopt SO_RCVBUF");
	}
#endif
}

static void tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0) {
		tdiff->tv_sec--; 
		tdiff->tv_usec += 1000000;
	}
}


/*
 *			N R E A D
 */
int Nread(int fd, char *buf, int count)
{
	struct sockaddr_in from;
	int len = sizeof(from);
	register int cnt;
	if( udp )  {
		cnt = recvfrom( fd, buf, count, 0, &from, &len );
		numCalls++;
	} else {
		if( b_flag )
			cnt = mread( fd, buf, count );	/* fill buf */
		else {
#ifdef LWK
			cnt = hio_read( fd, buf, count );
#else
			cnt = read( fd, buf, count );
#endif
			numCalls++;
		}
	}
	return(cnt);
}

/*
 *			N W R I T E
 */
int Nwrite(int fd, char *buf, int count)
{
	register int cnt;
	if( udp )  {
again:
		cnt = sendto( fd, buf, count, 0, &sinhim, sizeof(sinhim) );
		numCalls++;
		if( cnt<0 && errno == ENOBUFS )  {
			delay(18000);
			errno = 0;
			goto again;
		}
	} else {
		cnt = write( fd, buf, count );
		numCalls++;
	}
	return(cnt);
}

delay(us)
{
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = us;
	(void)select( 1, (char *)0, (char *)0, (char *)0, &tv );
	return(1);
}

/*
 *			M R E A D
 *
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because
 * network connections don't deliver data with the same
 * grouping as it is written with.  Written by Robert S. Miles, BRL.
 */
int
mread(fd, bufp, n)
int fd;
register char	*bufp;
unsigned	n;
{
	register unsigned	count = 0;
	register int		nread;

	do {
		nread = read(fd, bufp, n-count);
		numCalls++;
		if(nread < 0)  {
			perror("ttcp_mread");
			return(-1);
		}
		if(nread == 0)
			return((int)count);
		count += (unsigned)nread;
		bufp += nread;
	 } while(count < n);

	return((int)count);
}
