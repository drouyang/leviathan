#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include <stdint.h>

#include <pet_log.h>
#include "cmd_queue.h"
#include "xemem.h"


int main(int argc, char ** argv) {

    hcq_handle_t hcq = HCQ_INVALID_HANDLE;
    xemem_segid_t segid = atoll(argv[1]);
    hcq_cmd_t cmd = HCQ_INVALID_CMD;


    char * data_buf = "Hello Hobbes";

    hobbes_client_init();

    hcq = hcq_connect(segid);
    
    cmd = hcq_cmd_issue(hcq, 5, strlen(data_buf) + 1, data_buf);

    
    printf("cmd = %llu\n", cmd);
			    
    hobbes_client_deinit();

    return 0;

}
