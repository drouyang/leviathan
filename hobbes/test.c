#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include <pet_log.h>
#include "cmd_queue.h"
#include "xemem.h"

int main(int argc, char ** argv) {

    hcq_handle_t hcq = HCQ_INVALID_HANDLE;
    int fd = 0;
    fd_set rset;
    int max_fds = 0;
    int fd_cnt = 0;

    hobbes_client_init();

    hcq = hcq_create_queue();

    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not create command queue\n");\
	return -1;
    }

    printf("segid: %llu\n", hcq_get_segid(hcq));

    fd = hcq_get_fd(hcq);


    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    max_fds = fd + 1;
    
    while (1) {
	int ret = 0;

	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	max_fds = fd + 1;

	ret = select(max_fds, &rset, NULL, NULL, NULL);
    
	if (ret == -1) {
	    perror("Select Error");
	    break;
	}

	if (FD_ISSET(fd, &rset)) {
	    hcq_cmd_t cmd      = hcq_get_next_cmd(hcq);
	    uint64_t  cmd_code = hcq_get_cmd_code(hcq, cmd);
	    uint32_t  data_len = 0;
	    char    * data_buf = hcq_get_cmd_data(hcq, cmd, &data_len);

	    
	    printf("cmd code=%llu\n", cmd_code);
	    if (data_len > 0) {
		printf("data (%s)\n", data_buf);
	    }
	}



    }

    hobbes_client_deinit();
    

    return 0;


} 
