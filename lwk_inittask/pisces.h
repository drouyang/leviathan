
#ifndef __PISCES_H__
#define __PISCES_H__




#define PISCES_CTRL_PATH "/dev/pisces"


struct pisces_pci_spec {
    uint8_t name[128];
    uint32_t bus;
    uint32_t dev;
    uint32_t func;
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


struct pisces_file_pair {
    char lnx_file[128];
    char lwk_file[128];
} __attribute__((packed));


struct pisces_dbg_spec {
    uint32_t vm_id;
    uint32_t core;
    uint32_t cmd;
} __attribute__((packed));





/*** Local Kitten Requests for Pisces ***/

#define PISCES_STAT_FILE   500
#define PISCES_LOAD_FILE   501
#define PISCES_WRITE_FILE  502

struct pisces_user_file_info {
    unsigned long long user_addr;
    unsigned int       path_len;
    char               path[0];
} __attribute__((packed)); 



#endif
