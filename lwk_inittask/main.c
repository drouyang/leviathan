#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <lwk/liblwk.h>
#include <arch/types.h>
#include <lwk/pmem.h>
#include <lwk/smp.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <poll.h>

#include <stdint.h>

#include <xemem.h>
#include <pet_log.h>
#include <pet_hashtable.h>
#include <cmd_queue.h>

#include "pisces.h"
#include "palacios.h"
#include "job_launch.h"


cpu_set_t   enclave_cpus;
uint64_t    enclave_id = -1;


struct hashtable * cmd_handlers        = NULL;
struct hashtable * legacy_cmd_handlers = NULL;

	
int
main(int argc, char ** argv, char * envp[]) 
{
    hcq_handle_t hcq = HCQ_INVALID_HANDLE;
    fd_set       rset;    

    int hcq_fd    = 0;
    int pisces_fd = 0;
    int max_fd    = 0;


    CPU_ZERO(&enclave_cpus);	/* Initialize CPU mask */
    CPU_SET(0, &enclave_cpus);      /* We always boot on CPU 0 */


    printf("Pisces Control Daemon\n");

    // Get Enclave ID

    
    hobbes_client_init();

    
    hcq = hcq_create_queue();
    
    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not create command queue\n");
	return -1;
    }

    // register command queue w/ enclave

    

    while (1) {
	int ret = 0;
	
	
	//printf("Command=%llu, data_len=%d\n", cmd.cmd, cmd.data_len);
	
	switch (cmd.cmd) {
	    case ENCLAVE_CMD_ADD_MEM: {
		struct cmd_mem_add mem_cmd;
		struct pmem_region rgn;
		
		memset(&mem_cmd, 0, sizeof(struct cmd_mem_add));
		memset(&rgn, 0, sizeof(struct pmem_region));
		
		ret = read(pisces_fd, &mem_cmd, sizeof(struct cmd_mem_add));
		
		if (ret != sizeof(struct cmd_mem_add)) {
		    printf("Error reading pisces MEM_ADD CMD (ret=%d)\n", ret);
		    send_resp(pisces_fd, -1);
		    break;
		}
		
		
		rgn.start            = mem_cmd.phys_addr;
		rgn.end              = mem_cmd.phys_addr + mem_cmd.size;
		rgn.type_is_set      = 1;
		rgn.type             = PMEM_TYPE_UMEM;
		rgn.allocated_is_set = 1;
		rgn.allocated        = 0;
		
		printf("Adding pmem (%p - %p)\n", (void *)rgn.start, (void *)rgn.end);
		
		ret = pmem_add(&rgn);
		
		printf("pmem_add returned %d\n", ret);

		ret = pmem_zero(&rgn);

		printf("pmem_zero returned %d\n", ret);

		send_resp(pisces_fd, 0);

		break;
	    }
	    case ENCLAVE_CMD_ADD_CPU: {
		struct cmd_cpu_add cpu_cmd;
		int logical_cpu = 0;

		ret = read(pisces_fd, &cpu_cmd, sizeof(struct cmd_cpu_add));

		if (ret != sizeof(struct cmd_cpu_add)) {
		    printf("Error reading pisces CPU_ADD CMD (ret=%d)\n", ret);

		    send_resp(pisces_fd, -1);
		    break;
		}

		printf("Adding CPU phys_id %llu, apic_id %llu\n", 
		       (unsigned long long) cpu_cmd.phys_cpu_id, 
		       (unsigned long long) cpu_cmd.apic_id);

		logical_cpu = phys_cpu_add(cpu_cmd.phys_cpu_id, cpu_cmd.apic_id);

		if (logical_cpu == -1) {
		    printf("Error Adding CPU to Kitten\n");
		    send_resp(pisces_fd, -1);

		    break;
		}
			   

		v3_add_cpu(logical_cpu);

		CPU_SET(logical_cpu, &enclave_cpus);

		send_resp(pisces_fd, 0);
		break;
	    }
	    case ENCLAVE_CMD_REMOVE_CPU: {
		struct cmd_cpu_add cpu_cmd;
		int logical_cpu = 0;

		ret = read(pisces_fd, &cpu_cmd, sizeof(struct cmd_cpu_add));

		if (ret != sizeof(struct cmd_cpu_add)) {
		    printf("Error reading pisces CPU_ADD CMD (ret=%d)\n", ret);

		    send_resp(pisces_fd, -1);
		    break;
		}

		printf("Removing CPU phys_id %llu, apic_id %llu\n", 
		       (unsigned long long) cpu_cmd.phys_cpu_id, 
		       (unsigned long long) cpu_cmd.apic_id);

		logical_cpu = phys_cpu_remove(cpu_cmd.phys_cpu_id, cpu_cmd.apic_id);

		if (logical_cpu == -1) {
		    printf("Error remove CPU to Kitten\n");

		    send_resp(pisces_fd, -1);
		    break;
		}

		CPU_CLR(logical_cpu, &enclave_cpus);

		send_resp(pisces_fd, 0);
		break;
	    }

	    case ENCLAVE_CMD_LAUNCH_JOB: {
		struct cmd_launch_job * job_cmd = malloc(sizeof(struct cmd_launch_job));

		memset(job_cmd, 0, sizeof(struct cmd_launch_job));

		ret = read(pisces_fd, job_cmd, sizeof(struct cmd_launch_job));

		if (ret != sizeof(struct cmd_launch_job)) {
		    printf("Error reading Job Launch CMD (ret = %d)\n", ret);

		    free(job_cmd);
			    
		    send_resp(pisces_fd, -1);
		    break;
		}
			
		launch_job(pisces_fd, &(job_cmd->spec));

		free(job_cmd);
			
		send_resp(pisces_fd, 0);
		break;
	    }

	    case ENCLAVE_CMD_ADD_V3_PCI: {
		struct cmd_add_pci_dev cmd;
		//			    struct v3_hw_pci_dev   v3_pci_spec;
		int ret = 0;

		memset(&cmd, 0, sizeof(struct cmd_add_pci_dev));

		printf("Adding V3 PCI Device\n");

		ret = read(pisces_fd, &cmd, sizeof(struct cmd_add_pci_dev));

		if (ret != sizeof(struct cmd_add_pci_dev)) {
		    send_resp(pisces_fd, -1);
		    break;
		}

		//			    memcpy(v3_pci_spec.name, cmd.spec.name, 128);
		//			    v3_pci_spec.bus  = cmd.spec.bus;
		//    v3_pci_spec.dev  = cmd.spec.dev;
		// v3_pci_spec.func = cmd.spec.func;


		/* Issue Device Add operation to Palacios */
		/*
		  if (issue_v3_cmd(V3_ADD_PCI, (uintptr_t)&(v3_pci_spec)) == -1) {
		  printf("Error: Could not add PCI device to Palacios\n");
		  send_resp(pisces_fd, -1);
		  break;
		  }
		*/

		send_resp(pisces_fd, 0);
		break;
	    }
	 

	    case ENCLAVE_CMD_SHUTDOWN: {

		/*
		  if (issue_v3_cmd(V3_SHUTDOWN, 0) == -1) {
		  printf("Error: Could not shutdown Palacios VMM\n");
		  send_resp(pisces_fd, -1);
		  break;
		  }
		*/
		/* Perform additional Cleanup is necessary */

		send_resp(pisces_fd, 0);

		close(pisces_fd);
		exit(0);

	    }
	    default: {
		printf("Unknown Pisces Command (%llu)\n", cmd.cmd);
		send_resp(pisces_fd, -1);
		break;
	    }

	}
    }
    
    
    return 0;
 }
		


int
    register_cmd_handler() {

}
