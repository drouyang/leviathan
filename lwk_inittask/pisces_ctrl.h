
/* Pisces Legacy Commands
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __PISCES_CMDS_H__
#define __PISCES_CMDS_H__

#define ENCLAVE_CMD_ADD_CPU            100
#define ENCLAVE_CMD_ADD_MEM            101


#define ENCLAVE_CMD_REMOVE_CPU         110
#define ENCLAVE_CMD_REMOVE_MEM         111


#define ENCLAVE_CMD_CREATE_VM          120
#define ENCLAVE_CMD_FREE_VM            121
#define ENCLAVE_CMD_LAUNCH_VM          122
#define ENCLAVE_CMD_STOP_VM            123
#define ENCLAVE_CMD_PAUSE_VM           124
#define ENCLAVE_CMD_CONTINUE_VM        125
#define ENCLAVE_CMD_SIMULATE_VM        126

#define ENCLAVE_CMD_VM_MOVE_CORE       140
#define ENCLAVE_CMD_VM_DBG             141


#define ENCLAVE_CMD_VM_CONS_CONNECT    150
#define ENCLAVE_CMD_VM_CONS_DISCONNECT 151
#define ENCLAVE_CMD_VM_CONS_KEYCODE    152

#define ENCLAVE_CMD_ADD_V3_PCI         180
#define ENCLAVE_CMD_ADD_V3_SATA        181

#define ENCLAVE_CMD_FREE_V3_PCI        190



#define ENCLAVE_CMD_LAUNCH_JOB         200
#define ENCLAVE_CMD_LOAD_FILE          201
#define ENCLAVE_CMD_STORE_FILE         202

#define ENCLAVE_CMD_SHUTDOWN           900


#include "pisces.h"





struct pisces_cmd {
    uint64_t cmd;
    uint32_t data_len;
    uint8_t  data[0];
} __attribute__((packed));


struct pisces_resp {
    uint64_t status;
    uint32_t data_len;
    uint8_t  data[0];
} __attribute__((packed));


/* Linux -> Enclave Commands */


struct cmd_cpu_add {
    struct pisces_cmd hdr;
    uint64_t phys_cpu_id;
    uint64_t apic_id;
} __attribute__((packed));


struct cmd_mem_add {
    struct pisces_cmd hdr;
    uint64_t phys_addr;
    uint64_t size;
} __attribute__((packed));

struct pisces_job_spec {
    char name[64];
    char exe_path[256];
    char argv[256];
    char envp[256];

    union {
	uint64_t flags;
	struct {
	    uint64_t   use_large_pages : 1;
	    uint64_t   use_smartmap    : 1;
	    uint64_t   rsvd            : 62;
	} __attribute__((packed));
    } __attribute__((packed));

    uint8_t   num_ranks;
    uint64_t  cpu_mask;
    uint64_t  heap_size;
    uint64_t  stack_size;
} __attribute__((packed));

struct pisces_pci_spec {
    char     name[128];
    uint32_t bus;
    uint32_t dev;
    uint32_t func;
} __attribute__((packed));



struct pisces_file_pair {
    char lnx_file[128];
    char lwk_file[128];
} __attribute__((packed));


struct pisces_dbg_spec {
    uint32_t vm_id;
    uint32_t core;
    uint32_t cmd;
} __attribute__((packed));



struct vm_path {
    uint8_t file_name[256];
    uint8_t vm_name[128];
} __attribute__((packed));

struct cmd_create_vm {
    struct pisces_cmd hdr;
    struct vm_path    path;
} __attribute__((packed));



struct cmd_vm_ctrl {
    struct pisces_cmd hdr;

    uint32_t vm_id;
} __attribute__((packed));


struct cmd_vm_cons_keycode {
    struct pisces_cmd hdr;

    uint32_t vm_id;
    uint8_t  scan_code;
} __attribute__((packed));



struct cmd_vm_debug {
    struct pisces_cmd hdr;
    struct pisces_dbg_spec spec;
} __attribute__((packed));




struct cmd_add_pci_dev {
    struct pisces_cmd hdr;

    struct pisces_pci_spec spec;
} __attribute__((packed));



struct cmd_free_pci_dev {
    struct pisces_cmd hdr;

    struct pisces_pci_spec spec;
} __attribute__((packed));




struct cmd_launch_job {
    struct pisces_cmd      hdr;
    struct pisces_job_spec spec;
} __attribute__((packed));


struct cmd_load_file {
    struct pisces_cmd       hdr;
    struct pisces_file_pair file_pair;
} __attribute__((packed));



typedef int (*pisces_cmd_fn)(int        pisces_fd,
			     uint64_t   cmd);

int pisces_cmd_init(void);
int pisces_handle_cmd(int pisces_fd);
int register_pisces_cmd(uint64_t cmd, pisces_cmd_fn handler);


#endif
