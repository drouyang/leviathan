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

#include <pet_hashtable.h>
#include <pet_log.h>

#include <v3vee.h>
#include <v3_ioctl.h>

#include "init.h"
#include "pisces.h"
#include "pisces_ctrl.h"
#include "palacios.h"
#include "app_launch.h"
#include "hobbes_system.h"

static struct hashtable * pisces_cmd_handlers = NULL;

static uint32_t 
handler_hash_fn(uintptr_t key)
{
    return pet_hash_ptr(key);
}

static int
handler_equal_fn(uintptr_t key1, uintptr_t key2)
{
    return (key1 == key2);
}


int 
pisces_send_resp(int      fd, 
	  uint64_t err_code) 
{
	struct pisces_resp resp;

	resp.status   = err_code;
	resp.data_len = 0;
	
	write(fd, &resp, sizeof(struct pisces_resp));
	
	return 0;
}


static int 
issue_v3_cmd(u64 cmd, uintptr_t arg) 
{
        int palacios_fd = 0;
        int ret         = 0;
        
        palacios_fd = open(V3_DEV_FILENAME, O_RDWR);
        
        if (palacios_fd < 0) {
                printf("Could not open palacios CMD file (%s)\n", V3_DEV_FILENAME);
                return -1;
        }
        
        ret = ioctl(palacios_fd, cmd, arg);

        
        close(palacios_fd);

        return ret;
}




static int
__add_memory(int        pisces_fd, 
	     uint64_t   cmd)
{
    struct cmd_mem_add mem_cmd;
    struct pmem_region rgn;
    int ret = 0;

    memset(&mem_cmd, 0, sizeof(struct cmd_mem_add));
    memset(&rgn,     0, sizeof(struct pmem_region));

    ret = read(pisces_fd, &mem_cmd, sizeof(struct cmd_mem_add));

    if (ret != sizeof(struct cmd_mem_add)) {
	ERROR("Error reading pisces MEM_ADD CMD (ret=%d)\n", ret);
	pisces_send_resp(pisces_fd, -1);
	return 0;
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

    pisces_send_resp(pisces_fd, 0);

    return 0;
}


static int
__add_cpu(int      pisces_fd, 
	  uint64_t cmd)
{
    struct cmd_cpu_add cpu_cmd;
    int logical_cpu = 0;
    int ret         = 0;

    ret = read(pisces_fd, &cpu_cmd, sizeof(struct cmd_cpu_add));

    if (ret != sizeof(struct cmd_cpu_add)) {
	ERROR("Error reading pisces CPU_ADD CMD (ret=%d)\n", ret);

	pisces_send_resp(pisces_fd, -1);
	return 0;
    }

    printf("Adding CPU phys_id %llu, apic_id %llu\n", 
	   (unsigned long long) cpu_cmd.phys_cpu_id, 
	   (unsigned long long) cpu_cmd.apic_id);

    logical_cpu = phys_cpu_add(cpu_cmd.phys_cpu_id, cpu_cmd.apic_id);

    if (logical_cpu == -1) {
	ERROR("Error Adding CPU to Kitten\n");
	pisces_send_resp(pisces_fd, -1);

	return 0;
    }
			   
    
    if (palacios_enabled) {

	/* Notify Palacios of New CPU */

	if (issue_v3_cmd(V3_ADD_CPU, (uintptr_t)logical_cpu) == -1) {
	    ERROR("Error: Could not add CPU to Palacios\n");
	}
    }

    /* TODO: Move this hack into hobbes_ctrl.c one cpu assignment moves to Hobbes */
    if (hobbes_enabled) {
	hobbes_set_cpu_enclave_logical_id(cpu_cmd.phys_cpu_id, logical_cpu);
    }
			    
    CPU_SET(logical_cpu, &enclave_cpus);
    pisces_send_resp(pisces_fd, 0);

    return 0;
}

static int
__remove_cpu(int      pisces_fd, 
	     uint64_t cmd)
{
    struct cmd_cpu_add cpu_cmd;
    int logical_cpu = 0;
    int ret         = 0;

    ret = read(pisces_fd, &cpu_cmd, sizeof(struct cmd_cpu_add));

    if (ret != sizeof(struct cmd_cpu_add)) {
	ERROR("Error reading pisces CPU_ADD CMD (ret=%d)\n", ret);

	pisces_send_resp(pisces_fd, -1);
	return 0;
    }

    printf("Removing CPU phys_id %llu, apic_id %llu\n", 
	   (unsigned long long) cpu_cmd.phys_cpu_id, 
	   (unsigned long long) cpu_cmd.apic_id);

    logical_cpu = phys_cpu_remove(cpu_cmd.phys_cpu_id, cpu_cmd.apic_id);

    if (logical_cpu == -1) {
	ERROR("Error remove CPU to Kitten\n");

	pisces_send_resp(pisces_fd, -1);
	return 0;
    }

    CPU_CLR(logical_cpu, &enclave_cpus);

    pisces_send_resp(pisces_fd, 0);
    return 0;
}

static int
__launch_job(int      pisces_fd, 
	     uint64_t cmd)
{
    struct cmd_launch_job  * job_cmd  = (struct cmd_launch_job *)calloc(1, sizeof(struct cmd_launch_job));
    struct pisces_job_spec * job_spec = NULL;
    int ret = 0;
    static hobbes_id_t fake_hpid = 10000000UL;  /* HACK: start at high offset to avoid interferring with Leviathan */

    ret = read(pisces_fd, job_cmd, sizeof(struct cmd_launch_job));

    if (ret != sizeof(struct cmd_launch_job)) {
	ERROR("Error reading Job Launch CMD (ret = %d)\n", ret);

	free(job_cmd);
			    
	pisces_send_resp(pisces_fd, -1);
	return 0;
    }
    
    job_spec = &(job_cmd->spec);

    ret = launch_lwk_app(fake_hpid++,
			 job_spec->name,
			 job_spec->exe_path, 
			 job_spec->argv, 
			 job_spec->envp, 
			 (job_flags_t)(job_spec->flags), 
			 job_spec->num_ranks, 
			 job_spec->cpu_mask, 
			 0,  /* data_size */
			 job_spec->heap_size, 
			 job_spec->stack_size,
			 0, 0, 0);

    free(job_cmd);
			
    pisces_send_resp(pisces_fd, ret);

    return 0;
}




static int
__load_file(int      pisces_fd, 
	    uint64_t cmd)
{
    struct cmd_load_file * load_cmd = (struct cmd_load_file *)calloc(1, sizeof(struct cmd_load_file));
    int ret = 0;

    ret = read(pisces_fd, load_cmd, sizeof(struct cmd_load_file));

    if (ret != sizeof(struct cmd_load_file)) {
	printf("Error reading LOAD FILE CMD (ret = %d)\n", ret);

	free(load_cmd);
			    
	pisces_send_resp(pisces_fd, -1);
	return 0;
    }
    
    ret = load_remote_file(load_cmd->file_pair.lnx_file, 
			   load_cmd->file_pair.lwk_file);
    
    free(load_cmd);

    pisces_send_resp(pisces_fd, ret);
    return 0;
}

static int
__shutdown(int      pisces_fd, 
	   uint64_t cmd)
{
    if (palacios_enabled) {
	v3_shutdown();
    }

    /* Perform additional Cleanup is necessary */
    
    pisces_send_resp(pisces_fd, 0);

    sleep(5); /* Hack to avoid disabling the xbuf before the resp has been received */
	      /* TODO: Put the enclave into a halt loop */
    
    close(pisces_fd);
    exit(0);
}



int 
__handle_cmd(int    fd, 
	     void * priv_data)
{
    struct pisces_cmd cmd;
    pisces_cmd_fn     handler = NULL;

    int ret = 0;
 
    ret = read(fd, &cmd, sizeof(struct pisces_cmd));
	    
    if (ret != sizeof(struct pisces_cmd)) {
	printf("Error reading pisces CMD (ret=%d)\n", ret);
	return -1;
    }

    handler = (pisces_cmd_fn)pet_htable_search(pisces_cmd_handlers, (uintptr_t)cmd.cmd);

    if (handler == NULL) {
	ERROR("Received invalid pisces Command (%lu)\n", cmd.cmd);
	pisces_send_resp(fd, -1);
	return 0;
    }
    
    return handler(fd, cmd.cmd);

}



int 
pisces_cmd_init(int cmd_fd)
{
    pisces_cmd_handlers = pet_create_htable(0, handler_hash_fn, handler_equal_fn);
    
    if (pisces_cmd_handlers == NULL) {
	ERROR("Could not allocate Pisces Command hashtable\n");
	return -1;
    }	   

    register_pisces_cmd(PISCES_CMD_ADD_MEM,            __add_memory );
    register_pisces_cmd(PISCES_CMD_ADD_CPU,            __add_cpu    );
    register_pisces_cmd(PISCES_CMD_REMOVE_CPU,         __remove_cpu );
    register_pisces_cmd(PISCES_CMD_LAUNCH_JOB,         __launch_job );
    register_pisces_cmd(PISCES_CMD_LOAD_FILE,          __load_file  );
    register_pisces_cmd(PISCES_CMD_SHUTDOWN,           __shutdown   );

    add_fd_handler(cmd_fd, __handle_cmd, NULL);

    return 0;
}



int 
register_pisces_cmd(uint64_t        cmd, 
		    pisces_cmd_fn   handler_fn)
{
    if (pet_htable_search(pisces_cmd_handlers, cmd) != 0) {
	ERROR("Attempted to register duplicate command handler (cmd=%lu)\n", cmd);
	return -1;
    }

    if (pet_htable_insert(pisces_cmd_handlers, cmd, (uintptr_t)handler_fn) == 0) {
	ERROR("Could not register pisces command (cmd=%lu)\n", cmd);
	return -1;
    }

    return 0;

}




#if 0

			
		    case PISCES_CMD_CREATE_VM: {
			    struct pisces_user_file_info * file_info = NULL;
			    struct cmd_create_vm vm_cmd;
			    struct pmem_region rgn;
			    struct v3_guest_img guest_img;

			    id_t    my_aspace_id;
			    vaddr_t file_addr;
			    size_t  file_size =  0;
			    int     path_len  =  0;
			    int     vm_id     = -1;
			    int     status    =  0;


			    memset(&vm_cmd,    0, sizeof(struct cmd_create_vm));
			    memset(&rgn,       0, sizeof(struct pmem_region));
			    memset(&guest_img, 0, sizeof(struct v3_guest_img));
			    
			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_create_vm));

			    if (ret != sizeof(struct cmd_create_vm)) {
				    pisces_send_resp(pisces_fd, -1);
				    printf("Error: CREATE_VM command could not be read\n");
				    break;
			    }


			    path_len = strlen((char *)vm_cmd.path.file_name) + 1;

			    file_info = malloc(sizeof(struct pisces_user_file_info) + path_len);
			    memset(file_info, 0, sizeof(struct pisces_user_file_info) + path_len);

			    file_info->path_len = path_len;
			    strncpy(file_info->path, (char *)vm_cmd.path.file_name, path_len - 1);
			    
			    file_size = ioctl(pisces_fd, PISCES_STAT_FILE, file_info);

		
			    status = aspace_get_myid(&my_aspace_id);
			    if (status != 0) 
				return status;

			    if (pmem_alloc_umem(file_size, PAGE_SIZE, &rgn)) {
				printf("Error: Could not allocate umem for guest image (size=%lu)\n", file_size);
				break;
			    }
			    pmem_zero(&rgn);
				
			    status =
				aspace_map_region_anywhere(
							   my_aspace_id,
							   &file_addr,
							   round_up(file_size, PAGE_SIZE),
							   (VM_USER|VM_READ|VM_WRITE),
							   PAGE_SIZE,
							   "VM Image",
							   rgn.start
							   );


			    file_info->user_addr = file_addr;
		
			    ioctl(pisces_fd, PISCES_LOAD_FILE, file_info);
				
			    guest_img.size       = file_size;
			    guest_img.guest_data = (void *)file_info->user_addr;
			    strncpy(guest_img.name, (char *)vm_cmd.path.vm_name, 127);
				
				
			    /* Issue VM Create command to Palacios */
			    vm_id = issue_v3_cmd(V3_CREATE_GUEST, (uintptr_t)&guest_img);
				
				
			    aspace_unmap_region(my_aspace_id, file_addr, round_up(file_size, PAGE_SIZE));
			    pmem_free_umem(&rgn);
		

			    if (vm_id < 0) {
				printf("Error: Could not create VM (%s) at (%s) (err=%d)\n", 
				       vm_cmd.path.vm_name, vm_cmd.path.file_name, vm_id);
				pisces_send_resp(pisces_fd, vm_id);
				break;
			    }

			    printf("Created VM (%d)\n", vm_id);

			    pisces_send_resp(pisces_fd, vm_id);
			    break;
		    }
		    case PISCES_CMD_FREE_VM: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to Launch VM */
			    if (issue_v3_cmd(V3_FREE_GUEST, (uintptr_t)vm_cmd.vm_id) == -1) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    pisces_send_resp(pisces_fd, 0);

			    break;
		    }
		    case PISCES_CMD_ADD_V3_PCI: {
			    struct cmd_add_pci_dev cmd;
			    struct v3_hw_pci_dev   v3_pci_spec;
			    int ret = 0;

			    memset(&cmd, 0, sizeof(struct cmd_add_pci_dev));

			    printf("Adding V3 PCI Device\n");

			    ret = read(pisces_fd, &cmd, sizeof(struct cmd_add_pci_dev));

			    if (ret != sizeof(struct cmd_add_pci_dev)) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    memcpy(v3_pci_spec.name, cmd.spec.name, 128);
			    v3_pci_spec.bus  = cmd.spec.bus;
			    v3_pci_spec.dev  = cmd.spec.dev;
			    v3_pci_spec.func = cmd.spec.func;


			    /* Issue Device Add operation to Palacios */
			    if (issue_v3_cmd(V3_ADD_PCI, (uintptr_t)&(v3_pci_spec)) == -1) {
				    printf("Error: Could not add PCI device to Palacios\n");
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    pisces_send_resp(pisces_fd, 0);
			    break;
		    }
		    case PISCES_CMD_FREE_V3_PCI: {
			    struct cmd_add_pci_dev cmd;
			    struct v3_hw_pci_dev   v3_pci_spec;
			    int ret = 0;

			    memset(&cmd, 0, sizeof(struct cmd_add_pci_dev));

			    printf("Removing V3 PCI Device\n");

			    ret = read(pisces_fd, &cmd, sizeof(struct cmd_add_pci_dev));

			    if (ret != sizeof(struct cmd_add_pci_dev)) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    memcpy(v3_pci_spec.name, cmd.spec.name, 128);
			    v3_pci_spec.bus  = cmd.spec.bus;
			    v3_pci_spec.dev  = cmd.spec.dev;
			    v3_pci_spec.func = cmd.spec.func;


			    /* Issue Device Add operation to Palacios */
			    if (issue_v3_cmd(V3_REMOVE_PCI, (uintptr_t)&(v3_pci_spec)) == -1) {
				    printf("Error: Could not remove PCI device from Palacios\n");
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    pisces_send_resp(pisces_fd, 0);
			    break;
		    }
		    case PISCES_CMD_LAUNCH_VM: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to Launch VM */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_LAUNCH, (uintptr_t)NULL) == -1) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }


			    /*
			      if (xpmem_pisces_add_dom(palacios_fd, vm_cmd.vm_id)) {
			      printf("ERROR: Could not add connect to Palacios VM %d XPMEM channel\n", 
			      vm_cmd.vm_id);
			      }
			    */

			    pisces_send_resp(pisces_fd, 0);

			    break;
		    }
		    case PISCES_CMD_STOP_VM: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to Launch VM */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_STOP, (uintptr_t)NULL) == -1) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    pisces_send_resp(pisces_fd, 0);

			    break;
		    }

		    case PISCES_CMD_PAUSE_VM: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to Launch VM */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_PAUSE, (uintptr_t)NULL) == -1) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    pisces_send_resp(pisces_fd, 0);

			    break;
		    }
		    case PISCES_CMD_CONTINUE_VM: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to Launch VM */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_CONTINUE, (uintptr_t)NULL) == -1) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    pisces_send_resp(pisces_fd, 0);

			    break;
		    }
		    case PISCES_CMD_VM_CONS_CONNECT: {
			    struct cmd_vm_ctrl vm_cmd;
			    uint64_t cons_ring_buf = 0;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    printf("Error reading console command\n");

				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to connect the console */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_CONSOLE_CONNECT, (uintptr_t)&cons_ring_buf) == -1) {
				    cons_ring_buf        = 0;
			    }
					

			    printf("Cons Ring Buf=%p\n", (void *)cons_ring_buf);
			    pisces_send_resp(pisces_fd, cons_ring_buf);

			    break;
		    }

		    case PISCES_CMD_VM_CONS_DISCONNECT: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }


			    /* Send Disconnect Request to Palacios */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_CONSOLE_DISCONNECT, (uintptr_t)NULL) == -1) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    pisces_send_resp(pisces_fd, 0);
			    break;
		    }

		    case PISCES_CMD_VM_CONS_KEYCODE: {
			    struct cmd_vm_cons_keycode vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_cons_keycode));

			    if (ret != sizeof(struct cmd_vm_cons_keycode)) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Send Keycode to Palacios */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_KEYBOARD_EVENT, vm_cmd.scan_code) == -1) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }

			    pisces_send_resp(pisces_fd, 0);
			    break;
		    }

		    case PISCES_CMD_VM_DBG: {
			    struct cmd_vm_debug pisces_cmd;
			    struct v3_debug_cmd v3_cmd;
			    
			    ret = read(pisces_fd, &pisces_cmd, sizeof(struct cmd_vm_debug));
			    
			    if (ret != sizeof(struct cmd_vm_debug)) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }
			    
			    v3_cmd.core = pisces_cmd.spec.core;
			    v3_cmd.cmd  = pisces_cmd.spec.cmd;
			    
			    if (issue_vm_cmd(pisces_cmd.spec.vm_id, V3_VM_DEBUG, (uintptr_t)&v3_cmd) == -1) {
				    pisces_send_resp(pisces_fd, -1);
				    break;
			    }
			    
			    pisces_send_resp(pisces_fd, 0);
			    break;
		    }



		}


#endif
