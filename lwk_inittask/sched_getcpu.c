#include <stdlib.h>
#include <arch/vsyscall.h>

typedef long (*getcpu_p)(unsigned *cpu, void *unused1, void *unused2);

// This can go away when RHEL5 support is no longer needed
int 
sched_getcpu(void)
{
	getcpu_p getcpu =  (getcpu_p)VSYSCALL_ADDR(__NR_vgetcpu);
	unsigned cpu    =  0;
	int      status = -1;

	status = getcpu(&cpu, NULL, NULL);

	return (status == -1) ? status : (int)cpu;
}
