// Copyright 2007 Steven Gribble

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>

// only works on pentium+ x86
// access the pentium cycle counter
// this routine lifted from somewhere on the Web...
void access_counter(unsigned int *hi, unsigned int *lo) {
    asm("rdtsc; movl %%edx,%0; movl %%eax,%1"   /* Read cycle counter */
	: "=r" (*hi), "=r" (*lo)                /* and move results to */
	: /* No input */                        /* the two outputs */
	: "%edx", "%eax");
}

// here's the system call we'll use
#define DO_SYSCALL syscall(603)

// calculate difference (in microseconds) between two struct timevals
// assumes difference is less than 2^32 seconds, and unsigned int is 32 bits
unsigned int timediff(struct timeval before, struct timeval after) {
  unsigned int diff;

  diff = after.tv_sec - before.tv_sec;
  diff *= 1000000;
  diff += (after.tv_usec - before.tv_usec);

  return diff;
}

// measure the system call using the cycle counter.  measures the
// difference in time between doing two system calls and doing
// one system call, to try to factor out any measurement overhead

void measure_cyclecounter(float mhz) {
  unsigned int high_s, low_s, high_e, low_e;
  size_t nbytes;
  float latency_with_read, latency_no_read;

  // warm up all the caches by exercising the functions
  access_counter(&high_s, &low_s);
  // read(5, buf, 4);
  DO_SYSCALL;
  access_counter(&high_e, &low_e);

  // now do it for real
  access_counter(&high_s, &low_s);
  DO_SYSCALL;
  access_counter(&high_e, &low_e);
  latency_with_read = ((float) (low_e - low_s) / mhz);

  access_counter(&high_s, &low_s);
  access_counter(&high_e, &low_e);
  latency_no_read = ((float) (low_e - low_s) / mhz);

  // print out the results
  printf("(cyclecounter)  latency:  %f us\n", latency_with_read - latency_no_read);
}

// measure the system call using the cycle counter.  measures the
// difference in time between doing two*NLOOPS system calls and doing
// one*NLOOPS system calls, to try to factor out any measurement overhead

#define NUMLOOPS 1000000
void measure_gettimeofday() {
  struct timeval beforeone, afterone;
  struct timeval beforetwo, aftertwo;
  int loopcount;
  unsigned int diffone, difftwo, result;

  // warm up all caches
  gettimeofday(&beforeone, NULL);
  gettimeofday(&beforetwo, NULL);
  for (loopcount = 0; loopcount < NUMLOOPS; loopcount++) {
    DO_SYSCALL;
  }
  gettimeofday(&afterone, NULL);
  gettimeofday(&aftertwo, NULL);
  
  // measure loop of one syscall
  gettimeofday(&beforeone, NULL);
  for (loopcount = 0; loopcount < NUMLOOPS; loopcount++) {
    DO_SYSCALL;
  }
  gettimeofday(&afterone, NULL);

  // measure loop of two syscalls
  gettimeofday(&beforetwo, NULL);
  for (loopcount = 0; loopcount < NUMLOOPS; loopcount++) {
    DO_SYSCALL;
    DO_SYSCALL;
  }
  gettimeofday(&aftertwo, NULL);

  diffone = timediff(beforeone, afterone);
  difftwo = timediff(beforetwo, aftertwo);

  result = difftwo - diffone;
  printf("%f\n", ((float) result) / ((float) NUMLOOPS));
}

void usage(void) {
  fprintf(stderr, "usage: measure_syscall cpu_mhz\n");
  fprintf(stderr, "  e.g.,  measure_syscall 2791.375\n");
  exit(-1);
}



int main(int argc, char **argv) {
  int i;
  //float mhz;

  //if (argc < 2)
  //  usage();
  
  //if (sscanf(argv[1], "%f", &mhz) != 1)
  //  usage();

  //if ((mhz < 100.0)  || (mhz > 100000.0))
  //  usage();

  // measure usiing the cycle counter
  //measure_cyclecounter(mhz);

  // measure using "gettimeofday"
  printf("getpid  latency (us)\n");
  for (i = 0; i < 20; i++) {
	  measure_gettimeofday();
  }

  return 0;
}
