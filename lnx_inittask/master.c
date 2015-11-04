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
#include "hobbes_sys_db.h"
#include "hobbes_util.h"

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

extern hdb_db_t hobbes_master_db;

static void * create_master_db(void);
static int    export_master_db(void);
static int    populate_system_info(hdb_db_t db);


static int    reserve_cpu_list ( char * cpu_list );
static int    reserve_cpus     ( int num_cpus,     int numa_zone );
static int    reserve_memory   ( int mem_size_MB,  int numa_zone ); 

static void usage(char * exec_name) {
    printf("Usage: %s [options]\n"	\
	   " [-c, --cpu=<num_cores>]\n"	      	\
	   " [-n, --numa=<numa_zone>]\n"	\
	   " Alternatively a specific set of cpus can be specified with: \n" \
	   " [--cpulist=<cpus>]\n" \
	   " [-m, --mem=<size_in_MB>]\n",
	   exec_name
	   );
    exit(-1);
}


int master_init(int argc, char ** argv) {

    static int numa_zone_is_set = 0;
    static int cpu_list_is_set  = 0;
    static int cpu_str_is_set   = 0;
    static int mem_str_is_set   = 0;

    int      numa_zone   = -1;
    int      num_cpus    = -1;
    uint32_t mem_size_MB = -1;

    hdb_db_t  db       = NULL;
    char    * cpu_list = NULL;

    /* Create the master database */
    db = create_master_db();

    if (db == NULL) {
	printf("Error creating master database\n");
	return -1;
    }




    /* Begin the resource reservations */
    {
	char c = 0;
	int  opt_index = 0;

	static struct option long_options[] = {
	    {"cpu",      required_argument, &cpu_str_is_set,   'c'},
	    {"numa",     required_argument, &numa_zone_is_set, 'n'},
	    {"cpulist",  required_argument, &cpu_list_is_set,   1 },
	    {"mem",      required_argument, &mem_str_is_set,   'm'},
	    {0, 0, 0, 0}
	};

	while ((c = getopt_long_only(argc, argv, "c:n:m:", long_options, &opt_index)) != -1) {
	    switch (c) {
		case 'c':
		    num_cpus = smart_atoi(-1, optarg);
		    
		    if (num_cpus == -1) {
			ERROR("Invalid CPU argument (%s)\n", optarg);
			usage(argv[0]);
		    }

		    break;
		case 'n':
		    numa_zone = smart_atoi(-1, optarg);

		    if (numa_zone == -1) {
			ERROR("Invalid NUMA argument (%s)\n", optarg);
			usage(argv[0]);
		    }

		    break;
		case 'm':
		    mem_size_MB = smart_atoi(-1, optarg);

		    if (mem_size_MB == -1) {
			ERROR("Invalid Memory argument (%s)\n", optarg);
			usage(argv[0]);
		    }

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
	if (reserve_cpu_list(cpu_list) == -1) {
	    ERROR("Could not reserve CPU list (%s)\n", cpu_list);
	    return -1;
	}
    } else if (cpu_str_is_set) {
	if (reserve_cpus(num_cpus, numa_zone) == -1) {
	    ERROR("Could not reserve %d CPUs on NUMA zone %d\n", num_cpus, numa_zone);
	    return -1;
	}
    } else {
	/* Default to reserve CPU 0 */	
	if (reserve_cpu_list("0") == -1) {
	    ERROR("Could not reserve CPU 0 (default) for master enclave\n");
	    return -1;
	}
    }

    if (mem_str_is_set) {
	if (reserve_memory(mem_size_MB, numa_zone) == -1) {
	    ERROR("Could not reserve %uMB of memory on NUMA zone %d for master enclave\n", mem_size_MB, numa_zone);
	    return -1;
	}
    } else {
	/* Default to reserve 1GB on NUMA node 0 */
	if (reserve_memory(1024, 0) == -1) {
	    ERROR("Could not reserve default 1GB of memory on NUMA zone 0\n");
	    return -1;
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

    /* Initialize the system resource state */
    populate_system_info(db);


    /* Export Database */
    if (export_master_db() == -1) {
	ERROR("Could not export Master Database\n");
	return -1;
    }

    //    wg_print_db(db);

    return 0;
}


static int
reserve_cpu_list(char * cpu_list)
{
    uint64_t phys_cpu_id = 0;
    char   * iter_str    = NULL;	    
	
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

    return 0;
}


static int 
reserve_cpus(int num_cpus,
	     int numa_zone)
{
    struct pet_cpu * cpu_arr = NULL;

    int ret         = 0;
    int i           = 0;
    int cpu0_locked = 0;
	

    /* Linux usually reserves CPU 0 automatically. 
     * if so, and we are requesting CPU's in the same NUMA zone, we count it towards our total
     */
    if (pet_cpu_status(0) == PET_CPU_RSVD) {

	if ((numa_zone == -1) || 
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


    return 0;

}

static int    
reserve_memory(int mem_size_MB, 
	       int numa_zone)
{
    struct mem_block * blk_arr = NULL;

    uint32_t sys_blk_cnt = 0;
    uint32_t num_blks    = 0;
    int      blk_size_MB = pet_block_size() / (1024 * 1024);
    
    int ret = 0;
    int i   = 0;

    /* Rounding up allocation to match underlying block size */
    num_blks = mem_size_MB / blk_size_MB + ((mem_size_MB % blk_size_MB) != 0);
    
    printf("Reserving %u memory blocks (%uMB) for master enclave\n", 
	   num_blks, num_blks * blk_size_MB);

    /* Probe memory */
    ret = pet_probe_mem_node(&sys_blk_cnt, &blk_arr, numa_zone);
    
    if (ret == -1) {
	ERROR("Could not probe memory\n");
	return -1;
    }
    
    /* Subtract that which is already reserved */
    for (i = 0; i < sys_blk_cnt; i++) {
	if (blk_arr[i].numa_node != numa_zone) {
	    ERROR("NUMA zone mismatch. Bug in Petlib\n");
	    return -1;
	}

	if (blk_arr[i].state == PET_BLOCK_RSVD) {
	    num_blks--;
	}

	/* We've already reserved enough */
	if (num_blks == 0) {
	    return 0;
	}
    }

    /* reserve any more that we might need */

    

    for (i = 0; i < sys_blk_cnt; i++) {
	
	if (blk_arr[i].state == PET_BLOCK_ONLINE) {
	    pet_lock_block(blk_arr[i].base_addr / pet_block_size());
	    num_blks--;
	}

	/* We've reserved enough */
	if (num_blks == 0) {
	    return 0;
	}
    }

    


    return 0;
} 



static void * 
create_master_db(void) 
{

    hobbes_id_t     enclave_id = HOBBES_INVALID_ID;


    hobbes_master_db    = hdb_create(HDB_MASTER_DB_SIZE);


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
	

    return hobbes_master_db;
}





static int
export_master_db(void)
{
    xpmem_segid_t   segid      = HDB_MASTER_DB_SEGID;
    void          * db_addr    = NULL;

    db_addr = hdb_get_db_addr(hobbes_master_db);

    madvise(db_addr, HDB_MASTER_DB_SIZE, MADV_DONTFORK);

    printf("HDB SegID = %d, db_addr=%p, db_size=%d\n", (int)segid, db_addr, HDB_MASTER_DB_SIZE);
    //segid = xpmem_make(db_addr, HDB_MASTER_DB_SIZE, XPMEM_REQUEST_MODE, (void *)segid);
    segid = xpmem_make_ext(db_addr, HDB_MASTER_DB_SIZE, 
			   XPMEM_GLOBAL_MODE, (void *)0,
			   XPMEM_MEM_MODE | XPMEM_REQUEST_MODE, segid, 
			   NULL);

    if (segid <= 0) {
	printf("Error Creating SegID (%lld)\n", segid);
	return -1;
    } 

    printf("segid: %llu\n", segid);

    return 0;
}

static int 
populate_system_info(hdb_db_t db) 
{

    printf("Assigning Resources to Leviathan\n");

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
	
	uint32_t num_cpus  = 0;
	uint32_t free_cpus = 0;

	uint32_t i         = 0;

	int ret = 0;

	if (pet_probe_cpus(&num_cpus, &cpu_arr) != 0) {
	    ERROR("Error: Could not probe CPUs\n");
	    return -1;
	}
	
	for (i = 0; i < num_cpus; i++) {
	    cpu_state_t state      = CPU_INVALID;
	    hobbes_id_t enclave_id = HOBBES_INVALID_ID;


	    switch (cpu_arr[i].state) {
		case PET_CPU_ONLINE: {
		    int cpu_id = cpu_arr[i].cpu_id;
		    
		    pet_offline_cpu(cpu_id);
		    
		    if (pet_cpu_status(cpu_id) != PET_CPU_OFFLINE) {
			state      = CPU_ALLOCATED;
			enclave_id = HOBBES_MASTER_ENCLAVE_ID;
			break;
		    }
		    
		    free_cpus++;
		    state = CPU_FREE;
		    
		    break;
		}	
		case PET_CPU_RSVD:
		    state      = CPU_ALLOCATED;
		    enclave_id = HOBBES_MASTER_ENCLAVE_ID;
		    break;
		case PET_CPU_OFFLINE:
		case PET_CPU_INVALID:
		default: 
		    state = CPU_INVALID;
		    break;
	    }
	    
	    ret = hdb_register_cpu(db, cpu_arr[i].cpu_id, cpu_arr[i].numa_node, state, enclave_id);
	    
	    if (ret == -1) {
		ERROR("Error registering CPU with database\n");
		return -1;
	    }
	}

	
	printf("Registered %u CPUs (%u free) with Leviathan\n", num_cpus, free_cpus);

	free(cpu_arr);
    }


    /* Memory */
    {
	struct mem_block * blk_arr = NULL;

	uint32_t num_blks  = 0;
	uint32_t free_blks = 0;
	uint32_t i         = 0;



	int ret = 0;

	if (pet_probe_mem(&num_blks, &blk_arr) != 0) {
	    ERROR("Error: Could not probe memory\n");
	    return -1;
	}

	for (i = 0; i < num_blks; i++) {
	    mem_state_t state      = MEMORY_INVALID;
	    hobbes_id_t enclave_id = HOBBES_INVALID_ID;

	    switch (blk_arr[i].state) {
		case PET_BLOCK_ONLINE: {
		    /* 
		       Comment this out for now, otherwise it will break everything
		    */
		    int blk_index = blk_arr[i].base_addr / pet_block_size();
		    
		    if (pet_offline_block(blk_index) != 0) {
		  	state      = MEMORY_ALLOCATED;
			enclave_id = HOBBES_MASTER_ENCLAVE_ID;
			break;
		    }
		    
		    free_blks++;
		    state      = MEMORY_FREE;
		    
		    break;
		}

		case PET_BLOCK_RSVD: 
		    state      = MEMORY_ALLOCATED;
		    enclave_id = HOBBES_MASTER_ENCLAVE_ID;
		    break;
		case PET_BLOCK_OFFLINE:
		case PET_BLOCK_INVALID:
		default: 
		    state = MEMORY_INVALID;
		    break;
	    }

	    ret = hdb_register_memory(db, 
				      blk_arr[i].base_addr, 
				      blk_arr[i].pages * PAGE_SIZE, 
				      blk_arr[i].numa_node,
				      state, 
				      enclave_id);

	    if (ret == -1) {
		ERROR("Error registering memory with database\n");
		return -1;
	    }		      	    
	}
	
	printf("Registered %u memory blocks (%u free) with Leviathan\n", num_blks, free_blks);
	
	free(blk_arr);
    }


    return 0;

}
