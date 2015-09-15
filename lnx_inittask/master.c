#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>


#include <xpmem.h>
#include <pet_numa.h>
#include <pet_cpu.h>
#include <pet_mem.h>
#include <pet_hashtable.h>
#include <pet_log.h>

#include <v3vee.h>

#include "master.h"
#include "hobbes_ctrl.h"
#include "hobbes_db.h"

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

extern hdb_db_t hobbes_master_db;

static void * create_master_db(unsigned int size);



static void usage(char * exec_name) {
    printf("Usage: %s [options]\n"	\
	   " [-c, --cpu=<num_cores>]\n"	      	\
	   " [-n, --numa=<numa_zone>]\n"	\
	   " Alternatively a specific set of cpus can be specified with: \n" \
	   " [--cpulist=<cpus>]\n",
	   exec_name
	   );
    exit(-1);
}


static int 
populate_system_info(hdb_db_t db) 
{

    /* General Info */
    {
	uint32_t numa_cnt = pet_num_numa_nodes();
	uint64_t blk_size = pet_block_size();

	if ((blk_size == (uint64_t)-1) ||
	    (numa_cnt == (uint64_t)-1)) {
	    ERROR("Could not detect base system info (blk_size=%lu) (numa_cnt=%u)\n", blk_size, numa_cnt);
	    return -1;
	}
	
	hdb_init_system_info(db, numa_cnt, blk_size);
    }


    /* CPUs */
    {
	struct pet_cpu * cpu_arr  = NULL;
	
	uint32_t num_cpus = 0;
	uint32_t i        = 0;

	int ret = 0;

	if (pet_probe_cpus(&num_cpus, &cpu_arr) != 0) {
	    ERROR("Error: Could not probe CPUs\n");
	    return -1;
	}
	
	for (i = 0; i < num_cpus; i++) {
	    printf("CPU %d: N=%d, state=%d\n", cpu_arr[i].cpu_id, cpu_arr[i].numa_node, cpu_arr[i].state);

	    ret = hdb_register_cpu(db, cpu_arr[i].cpu_id, cpu_arr[i].numa_node, cpu_arr[i].state);
	    
	    if (ret == -1) {
		ERROR("Error register CPU with database\n");
		return -1;
	    }
	}
    }


    /* Memory */

    return 0;

}


int master_init(int argc, char ** argv) {
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



    db = create_master_db(HDB_MASTER_DB_SIZE);

    if (db == NULL) {
	printf("Error creating master database\n");
	return -1;
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


    //    wg_print_db(db);

    return 0;
}



static void * 
create_master_db(unsigned int size) 
{

    xpmem_segid_t   segid      = HDB_MASTER_DB_SEGID;
    hobbes_id_t     enclave_id = HOBBES_INVALID_ID;
    void          * db_addr    = NULL;


    hobbes_master_db    = hdb_create(size);


    /* Initialize Master DB State */
    hdb_init_master_db(hobbes_master_db);

    /* Create Master enclave */
    enclave_id = hdb_create_enclave(hobbes_master_db, "master", 0, MASTER_ENCLAVE, HOBBES_INVALID_ID);

    if (enclave_id == HOBBES_INVALID_ID) {
	ERROR("Could not register master enclave\n");
	return NULL;
    }

    printf("Master Enclave id = %d\n", enclave_id);
    printf("Master enclave name=%s\n", hdb_get_enclave_name(hobbes_master_db, enclave_id));

    hdb_set_enclave_state(hobbes_master_db, enclave_id, ENCLAVE_RUNNING);
	

    populate_system_info(hobbes_master_db);


    db_addr = hdb_get_db_addr(hobbes_master_db);

    madvise(db_addr, size, MADV_DONTFORK);

    printf("HDB SegID = %d, db_addr=%p, db_size=%d\n", (int)segid, db_addr, size);
    //segid = xpmem_make(db_addr, size, XPMEM_REQUEST_MODE, (void *)segid);
    segid = xpmem_make_ext(db_addr, size, 
			   XPMEM_GLOBAL_MODE, (void *)0,
			   XPMEM_MEM_MODE | XPMEM_REQUEST_MODE, segid, 
			   NULL);


    if (segid <= 0) {
	printf("Error Creating SegID (%lld)\n", segid);
	return NULL;
    } 

    printf("segid: %llu\n", segid);

    return hobbes_master_db;
}

