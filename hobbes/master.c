#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include <dbapi.h>
#include <dballoc.h>

#include <xpmem.h>
#include <pet_cpu.h>

#include "hobbes_db.h"

#define PAGE_SIZE sysconf(_SC_PAGESIZE)




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

    int ret = 0;


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
	
	while (iter_str = strsep(&cpu_list, ",")) {
	    phys_cpu_id = atoi(iter_str);
	    

	    if (pet_cpu_status(phys_cpu_id) == PET_CPU_OFFLINE) {
		fprintf(stderr, "ERROR: CPU %d is OFFLINE. Cannot lock.\n", phys_cpu_id);
		continue;
	    }

	    printf("Locking CPU %d\n", phys_cpu_id);
	    if (pet_lock_cpu(phys_cpu_id) == -1) {
		printf("Error: Could not Lock CPU %d's state\n", phys_cpu_id);
	    }

	    if (pet_cpu_status(phys_cpu_id) != PET_CPU_RSVD) {
		fprintf(stderr, "ERROR: Could not lock CPU %d\n", phys_cpu_id);
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

	int cpu_cnt = 0;
	int ret     = 0;
	int i       = 0;

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

    wg_print_db(db);


    

    while (1) {sleep(1);}

    return 0;
}



static void * 
create_master_db(unsigned int size) 
{

    xpmem_segid_t segid = HDB_MASTER_DB_SEGID;
    void *   db_addr    = NULL;
    hdb_db_t db         = hdb_create(size);
   
    /* Initialize Master DB State */
    hdb_init_master_db(db);

    /* Create Master enclave */
    hdb_create_enclave(db, "master", 0, MASTER_ENCLAVE, 0);


    printf("Master Enclave id = %d\n", 0);
    printf("Master enclave name=%s\n", hdb_get_enclave_name(db, 0));

    hdb_set_enclave_state(db, 0, ENCLAVE_RUNNING);
	

    /* Create Master Process */
       

    db_addr = hdb_get_db_addr(db);

    printf("HDB SegID = %d\n", (int)segid);
    //segid = xpmem_make(db_addr, size, XPMEM_REQUEST_MODE, (void *)segid);
    segid = xpmem_make_hobbes(db_addr, size, 
			      XPMEM_PERMIT_MODE, (void *)0600,
			      XPMEM_MEM_MODE | XPMEM_REQUEST_MODE, segid, 
			      NULL);


    if (segid <= 0) {
	printf("Error Creating SegID (%llu)\n", segid);
	wg_delete_local_database(db);
	return NULL;
    } 

    printf("segid: %llu\n", segid);

    return db;
}
