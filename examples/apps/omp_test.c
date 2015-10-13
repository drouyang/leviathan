#include <stdlib.h>
#include <stdio.h>
#include <omp.h>

enum vsyscall_num {
        __NR_vgettimeofday,
        __NR_vtime,
        __NR_vgetcpu,
};

#define VSYSCALL_START (-10UL << 20)
#define VSYSCALL_SIZE 1024
#define VSYSCALL_END (-2UL << 20)
#define VSYSCALL_MAPPED_PAGES 1
#define VSYSCALL_ADDR(vsyscall_nr) (VSYSCALL_START+VSYSCALL_SIZE*(vsyscall_nr))

typedef long (*getcpu_p)(unsigned *cpu, void *unused1, void *unused2);

// This can go away when RHEL5 support is no longer needed
int sched_getcpu()
{
        unsigned cpu;
        int status = -1;
        getcpu_p getcpu = (getcpu_p)VSYSCALL_ADDR(__NR_vgetcpu);

        status = getcpu(&cpu, NULL, NULL);

        return (status == -1) ? status : cpu;
}





int main(int argc, char ** argv) {
int nthreads, tid;


#pragma omp parallel private(nthreads, tid)
{
 	tid = omp_get_thread_num();

	printf("Hello from thread %d on CPU %d\n", tid, sched_getcpu());


	if (tid == 0) {
		printf("Number of threads=%d\n", omp_get_num_threads());
	}
}

	while (1) sleep(1);

}
