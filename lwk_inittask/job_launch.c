/* Kitten Job Launch 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */




#include <pet_log.h>

extern cpu_set_t enclave_cpus;


int 
launch_job(int pisces_fd, struct pisces_job_spec * job_spec)
{

    u32 page_size = (job_spec->use_large_pages ? VM_PAGE_2MB : VM_PAGE_4KB);

    vaddr_t   file_addr = 0;
    cpu_set_t spec_cpus;
    cpu_set_t job_cpus;
    user_cpumask_t lwk_cpumask;

    int status = 0;


    /* Figure out which CPUs are being requested */
    {
	int i = 0;

	CPU_ZERO(&spec_cpus);
	
	for (i = 0; i < 64; i++) {
	    if ((job_spec->cpu_mask & (0x1ULL << i)) != 0) {
		CPU_SET(i, &spec_cpus);
	    }
	}
    }




    /* Check if we can host the job on the current CPUs */
    /* Create a kitten compatible cpumask */
    {
	int i = 0;

	CPU_AND(&job_cpus, &spec_cpus, &enclave_cpus);

	if (CPU_COUNT(&job_cpus) < job_spec->num_ranks) {
	    printf("Error: Could not find enough CPUs for job\n");
	    return -1;
	}
	

	cpus_clear(lwk_cpumask);
	
	for (i = 0; (i < CPU_MAX_ID) && (i < CPU_SETSIZE); i++) {
	    if (CPU_ISSET(i, &job_cpus)) {
		cpu_set(i, lwk_cpumask);
	    }
	}



    }


    /* Load exe file info memory */
    {
	struct pisces_user_file_info * file_info = NULL;
	int    path_len  = strlen(job_spec->exe_path) + 1;
	size_t file_size = 0;
	id_t   my_aspace_id;

	file_info = malloc(sizeof(struct pisces_user_file_info) + path_len);
	memset(file_info, 0, sizeof(struct pisces_user_file_info) + path_len);
    
	file_info->path_len = path_len;
	strncpy(file_info->path, job_spec->exe_path, path_len - 1);
    
	file_size = ioctl(pisces_fd, PISCES_STAT_FILE, file_info);
    
	status = aspace_get_myid(&my_aspace_id);
	if (status != 0) 
	    return status;


	{
	
	    paddr_t pmem = elf_dflt_alloc_pmem(file_size, page_size, 0);

	    printf("PMEM Allocated at %p (file_size=%lu) (page_size=0x%x) (pmem_size=%p)\n", 
		   (void *)pmem, 
		   file_size,
		   page_size, 
		   (void *)round_up(file_size, page_size));

	    if (pmem == 0) {
		printf("Could not allocate space for exe file\n");
		return -1;
	    }

	    /* Map the segment into this address space */
	    status =
		aspace_map_region_anywhere(
					   my_aspace_id,
					   &file_addr,
					   round_up(file_size, page_size),
					   (VM_USER|VM_READ|VM_WRITE),
					   page_size,
					   "File",
					   pmem
					   );
	    if (status)
		return status;
	

	    file_info->user_addr = file_addr;
	}
    
	printf("Loading EXE into memory\n");

	ioctl(pisces_fd, PISCES_LOAD_FILE, file_info);


	free(file_info);
    }


    printf("Job Launch Request (%s) [%s %s]\n", job_spec->name, job_spec->exe_path, job_spec->argv);


    /* Initialize start state for each rank */
    {
	start_state_t * start_state = NULL;
	int rank = 0; 

	/* Allocate start state for each rank */
	start_state = malloc(job_spec->num_ranks * sizeof(start_state_t));
	if (!start_state) {
	    printf("malloc of start_state[] failed\n");
	    return -1;
	}


	for (rank = 0; rank < job_spec->num_ranks; rank++) {
	    int cpu = 0;
	    int i   = 0;
	    
	    for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, &job_cpus)) {
		    CPU_CLR(i, &job_cpus);
		    cpu = i;
		    break;
		}
	    }


	    printf("Loading Rank %d on CPU %d\n", rank, cpu);
	    
	    start_state[rank].task_id  = ANY_ID;
	    start_state[rank].cpu_id   = ANY_ID;
	    start_state[rank].user_id  = 1;
	    start_state[rank].group_id = 1;
	    
	    sprintf(start_state[rank].task_name, job_spec->name);


	    status = elf_load((void *)file_addr,
			      job_spec->name,
			      ANY_ID,
			      page_size, 
			      job_spec->heap_size,   // heap_size 
			      job_spec->stack_size,  // stack_size 
			      job_spec->argv,        // argv_str
			      job_spec->envp,        // envp_str
			      &start_state[rank],
			      0,
			      &elf_dflt_alloc_pmem
			      );
	    

	    if (status) {
		printf("elf_load failed, status=%d\n", status);
	    }
	    

	    if ( aspace_update_user_cpumask(start_state[rank].aspace_id, &lwk_cpumask) != 0) {
		printf("Error updating CPU mask\n");
		return -1;
	    }
		 

	    /* Setup Smartmap regions if enabled */
	    if (job_spec->use_smartmap) {
		int src = 0;
		int dst = 0;

		printf("Creating SMARTMAP mappings...\n");
		for (dst = 0; dst < job_spec->num_ranks; dst++) {
		    for (src = 0; src < job_spec->num_ranks; src++) {
			status =
			    aspace_smartmap(
					    start_state[src].aspace_id,
					    start_state[dst].aspace_id,
					    SMARTMAP_ALIGN + (SMARTMAP_ALIGN * src),
					    SMARTMAP_ALIGN
					    );
			
			if (status) {
			    printf("smartmap failed, status=%d\n", status);
			    return -1;
			}
		    }
		}
		printf("    OK\n");
	    }

	    
	    printf("Creating Task\n");
	    
	    status = task_create(&start_state[rank], NULL);
	}
    }
    

    
    return 0;
}
