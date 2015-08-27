#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <hobbes_db.h>
#include <hobbes_file.h>

#include <pet_xml.h>
#include <pet_log.h>


#include "enclave.h"
#include "vm.h"
#include "pisces.h"


extern hdb_db_t hobbes_master_db;

int 
hobbes_create_enclave(char * cfg_file_name, 
		      char * name)
{
    pet_xml_t   xml  = NULL;
    char      * type = NULL; 

    xml = pet_xml_open_file(cfg_file_name);
    
    if (xml == NULL) {
	ERROR("Error loading Enclave config file (%s)\n", cfg_file_name);
	return -1;
    }

    type =  pet_xml_tag_name(xml);

    if (strncmp("pisces", type, strlen("pisces")) != 0) {
	ERROR("Invalid Enclave Type (%s)\n", type);
	return -1;
    }

    DEBUG("Creating Pisces Enclave\n");
    return pisces_enclave_create(xml, name);
}



int 
hobbes_destroy_enclave(hobbes_id_t enclave_id)
{
    enclave_type_t enclave_type = INVALID_ENCLAVE;

    enclave_type = hdb_get_enclave_type(hobbes_master_db, enclave_id);

    if (enclave_type == INVALID_ENCLAVE) {
	ERROR("Could not find enclave (%d)\n", enclave_id);
	return -1;
    }
   
    if (enclave_type != PISCES_ENCLAVE) {
	ERROR("Enclave (%d) is not a native enclave\n", enclave_id);
	return -1;
    }

    return pisces_enclave_destroy(enclave_id);
}



int 
create_enclave_main(int argc, char ** argv)
{
    char * cfg_file = NULL;
    char * name     = NULL;


    if (argc < 1) {
	printf("Usage: hobbes create_enclave <cfg_file> [name]\n");
	return -1;
    }

    cfg_file = argv[1];
    
    if (argc >= 2) {
	name = argv[2];
    }

    return hobbes_create_enclave(cfg_file, name);
}


int 
destroy_enclave_main(int argc, char ** argv)
{
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;

    if (argc < 1) {
	printf("Usage: hobbes destroy_enclave <enclave name>\n");
	return -1;
    }
    
    enclave_id = hobbes_get_enclave_id(argv[1]);
    

    return hobbes_destroy_enclave(enclave_id);
}


int
ping_enclave_main(int argc, char ** argv)
{
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;
    
    if (argc < 1) {
	printf("Usage: hobbes ping_enclave <enclave name>\n");
	return -1;
    }

    enclave_id = hobbes_get_enclave_id(argv[1]);

    if (enclave_id == HOBBES_INVALID_ID) {
	printf("Invalid Enclave\n");
	return -1;
    }

    return hobbes_ping_enclave(enclave_id);
}



int 
cat_file_main(int argc, char ** argv) 
{
    hobbes_id_t   enclave_id = HOBBES_INVALID_ID;
    hcq_handle_t  hcq        = HCQ_INVALID_HANDLE; 
    hobbes_file_t hfile      = HOBBES_INVALID_FILE;
    
    if (argc < 2) {
	printf("Usage: hobbes cat_file <enclave name> <path>\n");
	return -1;
    }

    enclave_id = hobbes_get_enclave_id(argv[1]);

    if (enclave_id == HOBBES_INVALID_ID) {
	ERROR("Invalid Enclave name (%s)\n", argv[1]);
	return -1;
    }

    hcq = hobbes_open_enclave_cmdq(enclave_id);

    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not open command queue for enclave (%s)\n", argv[1]);
	return -1;
    }
    
    hfile = hfio_open(hcq, argv[2], O_RDONLY);

    if (hfile == HOBBES_INVALID_FILE) {
	ERROR("Could not open file (%s)\n", argv[2]);
	return -1;
    }

    {
	char * tmp_buf = NULL;

	tmp_buf = calloc(HFIO_MAX_XFER_SIZE + 1, 1);

	if (tmp_buf == NULL) {
	    ERROR("Could not allocate temporary read buffer\n");
	    return -1;
	}

	while (1) {
	    ssize_t bytes_read = 0;
	    
	    bytes_read = hfio_read(hfile, tmp_buf, HFIO_MAX_XFER_SIZE);

	    if (bytes_read == 0) {
		break;
	    }

	    tmp_buf[bytes_read] = 0;

	    printf("%s", tmp_buf);
	}

	free(tmp_buf);
    }
    
    hfio_close(hfile);

    hobbes_close_enclave_cmdq(hcq);

    return 0;
}


static void
sig_term_handler(int sig)
{
    return;
}


int
cat_into_file_main(int argc, char ** argv)
{
    hobbes_id_t   enclave_id    = HOBBES_INVALID_ID;
    hcq_handle_t  hcq           = HCQ_INVALID_HANDLE; 
    hobbes_file_t hfile         = HOBBES_INVALID_FILE;

    /* Prevent SIGINT from killing, rely on ctrl-d */
    {
        struct sigaction action;
        
        memset(&action, 0, sizeof(struct sigaction));
        action.sa_handler = sig_term_handler;
        
        if (sigaction(SIGINT, &action, 0)) {
            perror("sigaction");
            return -1;
        }
    }

 if (argc < 2) {
	printf("Usage: hobbes cat_file <enclave name> <path>\n");
	return -1;
    }

    enclave_id = hobbes_get_enclave_id(argv[1]);

    if (enclave_id == HOBBES_INVALID_ID) {
	ERROR("Invalid Enclave name (%s)\n", argv[1]);
	return -1;
    }

    hcq = hobbes_open_enclave_cmdq(enclave_id);

    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not open command queue for enclave (%s)\n", argv[1]);
	return -1;
    }
    
    hfile = hfio_open(hcq, argv[2], O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IROTH);

    if (hfile == HOBBES_INVALID_FILE) {
	ERROR("Could not open file (%s)\n", argv[2]);
	return -1;
    }

    {
	char tmp_buf[1024] = {[0 ... 1023] = 0};

	while (1) {
	    int     offset         = 0;
	    ssize_t bytes_read     = read(STDIN_FILENO, tmp_buf, sizeof(tmp_buf));
	    size_t  bytes_to_write = bytes_read;
	
	    if (bytes_read <= 0) {
		if (errno == EAGAIN)  continue;
		break;
	    }

	    printf("Sending %lu bytes\n", bytes_to_write);
	    while (bytes_to_write > 0) {
		ssize_t bytes_wrote = hfio_write(hfile, tmp_buf + offset, bytes_to_write);
		
		printf("wrote %ld bytes\n", bytes_wrote);

		if (bytes_wrote <= 0) {
		    break;
		}
	
		bytes_to_write -= bytes_wrote;
		offset         += bytes_wrote;
	    }
	
	    printf("sent\n");

	    if (bytes_to_write > 0) {
		break;
	    }
	}
    }

    hfio_close(hfile);

    hobbes_close_enclave_cmdq(hcq);

    return 0;
}


int 
dump_cmd_queue_main(int argc, char ** argv)
{
    hobbes_id_t  enclave_id = HOBBES_INVALID_ID;
    hcq_handle_t hcq        = HCQ_INVALID_HANDLE;

    if (argc < 1) {
	printf("Usage: hobbes ping_enclave <enclave name>\n");
	return -1;
    }

    enclave_id = hobbes_get_enclave_id(argv[1]);

    if (enclave_id == HOBBES_INVALID_ID) {
	printf("Invalid Enclave\n");
	return -1;
    }
  
    hcq = hobbes_open_enclave_cmdq(enclave_id);
    
    hcq_dump_queue(hcq);

    hobbes_close_enclave_cmdq(hcq);

    return 0;
}

int
list_enclaves_main(int argc, char ** argv)
{
    struct enclave_info * enclaves = NULL;
    int num_enclaves = -1;
    int i = 0;

    enclaves = hobbes_get_enclave_list(&num_enclaves);

    if (enclaves == NULL) {
	ERROR("Could not retrieve enclave list\n");
	return -1;
    }
	
    printf("%d Active Enclaves:\n", num_enclaves);
    printf("--------------------------------------------------------------------------------\n");
    printf("| ID       | Enclave name                     | Type             | State       |\n");
    printf("--------------------------------------------------------------------------------\n");

 
    for (i = 0; i < num_enclaves; i++) {
	printf("| %-*d | %-*s | %-*s | %-*s |\n", 
	       8, enclaves[i].id,
	       32, enclaves[i].name,
	       16, enclave_type_to_str(enclaves[i].type), 
	       11, enclave_state_to_str(enclaves[i].state));
    }

    printf("--------------------------------------------------------------------------------\n");

    free(enclaves);

    return 0;
}


