#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>

#include <dbapi.h>
#include <dballoc.h>

#include <xpmem.h>
#include <pet_cpu.h>
#include <pet_hashtable.h>
#include <pet_log.h>

#include <v3vee.h>

#include "master.h"
#include "hobbes_ctrl.h"
#include "hobbes_db.h"

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

extern hdb_db_t hobbes_master_db;


static uint32_t           handler_max_fd = 0;
static fd_set             handler_fdset;
static struct hashtable * handler_table  = NULL;

struct fd_handler {
    int             fd;
    fd_handler_fn   fn;
    void          * priv_data;
};


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




static void * create_master_db(unsigned int size);



static void usage(char * exec_name) {
    printf("Usage: %s [options]\n"		\
	   " [-c, --cpu=<num_cores>]\n"	      	\
	   " [-n, --numa=<numa_zone>]\n"	\
	   " Alternatively a specific set of cpus can be specified with: \n" \
	   " [--cpulist=<cpus>]\n",
	   exec_name
	   );
    exit(-1);
}



int main(int argc, char ** argv) {
    int numa_zone   = -1;
    
    static int numa_zone_is_set = 0;
    static int cpu_list_is_set  = 0;
    static int cpu_str_is_set   = 0;

    void * db       = NULL;
    char * cpu_str  = NULL;
    char * cpu_list = NULL;



   /* Parse options */
    {
	char c = 0;
	int  opt_index = 0;

	static struct option long_options[] = {
	    {"cpu",      required_argument, &cpu_str_is_set,   'c'},
	    {"numa",     required_argument, &numa_zone_is_set, 'n'},
	    {"cpulist",  required_argument, &cpu_list_is_set,   1 },
	    {0, 0, 0, 0}
	};

	while ((c = getopt_long_only(argc, argv, "c:n:", long_options, &opt_index)) != -1) {
	    switch (c) {
		case 'c':
		    cpu_str = optarg;
		    break;
		case 'n':
		    numa_zone = atoi(optarg);
		    break;
		case 0: {
		    switch (opt_index) {
			case 2:
			    cpu_list = optarg;
			    break;
		    }
		    break;
		}
		case '?':
		default:
		    usage(argv[0]);
		    break;
	    }
	}

    }


    if ((cpu_list_is_set != 0) && 
	(cpu_str_is_set  != 0)) {

	fprintf(stderr, "Error: Cannot use both --cpu and --cpulist options at the same time\n");
	exit(-1);
    } 
	
    if (cpu_list_is_set) {
	uint64_t phys_cpu_id = 0;
	char   * iter_str    = NULL;	    
	
	if (numa_zone_is_set != 0) {
	    printf("Ignoring NUMA specification\n");
	}
	
	while ((iter_str = strsep(&cpu_list, ","))) {
	    phys_cpu_id = atoi(iter_str);
	    

	    if (pet_cpu_status(phys_cpu_id) == PET_CPU_OFFLINE) {
		fprintf(stderr, "ERROR: CPU %lu is OFFLINE. Cannot lock.\n", phys_cpu_id);
		continue;
	    }

	    printf("Locking CPU %lu\n", phys_cpu_id);
	    if (pet_lock_cpu(phys_cpu_id) == -1) {
		printf("Error: Could not Lock CPU %lu's state\n", phys_cpu_id);
	    }

	    if (pet_cpu_status(phys_cpu_id) != PET_CPU_RSVD) {
		fprintf(stderr, "ERROR: Could not lock CPU %lu\n", phys_cpu_id);
		continue;
	    }

	}
	
    } else if (cpu_str) {
	struct pet_cpu * cpu_arr = NULL;

	int num_cpus    = atoi(cpu_str);
	int ret         = 0;
	int i           = 0;
	int cpu0_locked = 0;
	

	/* Linux usually reserves CPU 0 automatically. 
	 * if so, and we are requesting CPU's in the same NUMA zone, we count it towards our total
	 */
	if (pet_cpu_status(0) == PET_CPU_RSVD) {

	    if ((!numa_zone_is_set) || 
		(pet_cpu_to_numa_node(0) == numa_zone)) {
		cpu0_locked = 1;
	    }

	}

	if (num_cpus > 0) {

	    cpu_arr = calloc(num_cpus - cpu0_locked, sizeof(struct pet_cpu));
	    
	    ret = pet_lock_cpus_on(num_cpus - cpu0_locked, numa_zone, cpu_arr);
	    
	    if (ret != (num_cpus - cpu0_locked)) {
		printf("Error: Could not Reserve %d CPUs for Management enclave\n", num_cpus);
		
		pet_unlock_cpus(ret, cpu_arr);
		free(cpu_arr);
		
		return -1;
	    }
	    
	    
	    printf("Reserved CPUS (");

	    if (cpu0_locked) {
		printf("0");
		
		if (num_cpus > 1) {
		    printf(", ");
		}
	    }
	    

	    for (i = 0; i < num_cpus - cpu0_locked; i++) {
		if (i != 0) printf(", ");
		
		printf("%d", cpu_arr[i].cpu_id);
	    }
	    printf(") for managment enclave\n");
	    
	    
	    free(cpu_arr);
	}
    } else {
	/* By default we lock CPU 0, unless it is already locked */

	if (pet_cpu_status(0) != PET_CPU_RSVD) {
	    pet_lock_cpu(0);
	}

    }


    /* 
     * Configure allocation policies to avoid non-reserved CPUs
     */
    {
	struct pet_cpu * cpu_arr = NULL;

	uint32_t cpu_cnt = 0;
	uint32_t i       = 0;
	int      ret     = 0;

	ret = pet_probe_cpus(&cpu_cnt, &cpu_arr);

	if (ret != 0) {
	    fprintf(stderr, "Error: Could not probe CPUs\n");
	    exit(-1);
	}


	for (i = 0; i < cpu_cnt; i++) {
	    if (cpu_arr[i].state == PET_CPU_OFFLINE) {
		fprintf(stderr, "Error: Encountered Offline CPU [%d].\n", cpu_arr[i].cpu_id);
		fprintf(stderr, "\tSystem maybe in an inconsistent state\n");
		continue;
	    }

	    if (cpu_arr[i].state == PET_CPU_INVALID) {
		fprintf(stderr, "Error: Encountered INVALID CPU [%d].\n", cpu_arr[i].cpu_id);
		fprintf(stderr, "\tSystem management maybe BUGGY\n");
		continue;
	    }
		
	    if (v3_is_vmm_present()) {
		if (cpu_arr[i].state != PET_CPU_RSVD) {
		    printf("Removing CPU %d from Palacios/Linux\n", cpu_arr[i].cpu_id);
		    v3_remove_cpu(cpu_arr[i].cpu_id);
		}
	    }
	}
    }

    db = create_master_db(HDB_MASTER_DB_SIZE);

    if (db == NULL) {
	printf("Error creating master database\n");
	return -1;
    }

    //    wg_print_db(db);


    /* Create command queue */

    {
	handler_table = pet_create_htable(0, handler_hash_fn, handler_equal_fn);

	if (handler_table == NULL) {
	    ERROR("Could not create FD handler hashtable\n");
	    return -1;
	}

	if (hobbes_ctrl_init() == -1) {
	    ERROR("Could not initialize hobbes environment\n");
	    exit(-1);
	}
	
    }


    /* Command Loop */
    printf("Entering Command Loop\n");

    while (1) {
	int    i    = 0;
	int    ret  = 0;	
	fd_set rset = handler_fdset;

	ret = select(handler_max_fd + 1, &rset, NULL, NULL, NULL);

	printf("select returned (ret=%d)\n", ret);
	if (ret == -1) {
	    ERROR("Select() error\n");
	    break;
	}

	for (i = 0; i <= handler_max_fd; i++) {
	    if (FD_ISSET(i, &rset)) {
		printf("%d.", i);
	    }
	}
	printf("\n");


	for (i = 0; i <= handler_max_fd; i++) {
	    if (FD_ISSET(i, &rset)) {
		struct fd_handler * handler = NULL;
		int ret = 0;

		handler = (struct fd_handler *)pet_htable_search(handler_table, i);

		if (handler == NULL) {
		    ERROR("FD is set, but there is not handler associated with it...\n");
		    continue;
		}
		
		assert(handler->fd == i);

		ret = handler->fn(i, handler->priv_data);
		
		if (ret != 0) {
		    ERROR("Error in fd handler for FD (%d)\n", i);
		    ERROR("Removing FD from handler set\n");
		    FD_CLR(i, &handler_fdset);
		    continue;
		}

	    }
	    
	}
	
    }

    return 0;
}



static void * 
create_master_db(unsigned int size) 
{

    xpmem_segid_t segid = HDB_MASTER_DB_SEGID;
    void *   db_addr    = NULL;
    


    hobbes_master_db    = hdb_create(size);


    /* Initialize Master DB State */
    hdb_init_master_db(hobbes_master_db);

    /* Create Master enclave */
    hdb_create_enclave(hobbes_master_db, "master", 0, MASTER_ENCLAVE, 0);


    printf("Master Enclave id = %d\n", 0);
    printf("Master enclave name=%s\n", hdb_get_enclave_name(hobbes_master_db, 0));

    hdb_set_enclave_state(hobbes_master_db, 0, ENCLAVE_RUNNING);
	

    /* Create Master Process */
       

    db_addr = hdb_get_db_addr(hobbes_master_db);

    madvise(db_addr, size, MADV_DONTFORK);

    printf("HDB SegID = %d\n", (int)segid);
    //segid = xpmem_make(db_addr, size, XPMEM_REQUEST_MODE, (void *)segid);
    segid = xpmem_make_ext(db_addr, size, 
			      XPMEM_GLOBAL_MODE, (void *)0,
			      XPMEM_MEM_MODE | XPMEM_REQUEST_MODE, segid, 
			      NULL);


    if (segid <= 0) {
	printf("Error Creating SegID (%llu)\n", segid);
	wg_delete_local_database(hobbes_master_db);
	return NULL;
    } 

    printf("segid: %llu\n", segid);

    return hobbes_master_db;
}


int
add_fd_handler(int             fd,
	       fd_handler_fn   fn,
	       void          * priv_data)
{
    struct fd_handler * new_handler = NULL;

    if (pet_htable_search(handler_table, fd) != 0) {
	ERROR("Attempted to register a duplicate FD handler (fd=%d)\n", fd);
	return -1;
    }

    new_handler = calloc(sizeof(struct fd_handler), 1);
    
    if (new_handler == NULL) {
	ERROR("Could not allocate FD handler state\n");
	return -1;
    }

    new_handler->fn        = fn;
    new_handler->fd        = fd;
    new_handler->priv_data = priv_data;

    if (pet_htable_insert(handler_table, fd, (uintptr_t)new_handler) == 0) {
	ERROR("Could not register FD handler (fd=%d)\n", fd);
	free(new_handler);
	return -1;
    }

    FD_SET(fd, &handler_fdset);

    if (handler_max_fd < fd) handler_max_fd = fd;

    return 0;
}


int
remove_fd_handler(int fd)
{
    struct fd_handler * handler = NULL;

    FD_CLR(fd, &handler_fdset);
    
    handler = (struct fd_handler *)pet_htable_search(handler_table, fd);

    if (handler == NULL) {
	ERROR("Could not find handler for FD (%d)\n", fd);
	return -1;
    }

    handler = (struct fd_handler *)pet_htable_remove(handler_table, fd, 0);

    if (handler == NULL) {
	ERROR("Could not remove handler from the FD hashtable (fd=%d)\n", fd);
	return -1;
    }

    free(handler);

    return 0;
}
