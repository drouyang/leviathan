/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */


#ifndef __HOBBES_CMDS_H__
#define __HOBBES_CMDS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* LWK Commands */
#define HOBBES_CMD_ADD_CPU             1000
#define HOBBES_CMD_ADD_MEM             1001 
#define HOBBES_CMD_ADD_PCI             1002

#define HOBBES_CMD_REMOVE_CPU          1010
#define HOBBES_CMD_REMOVE_MEM          1011
#define HOBBES_CMD_REMOVE_PCI          1012

#define HOBBES_CMD_LOAD_FILE           1100
#define HOBBES_CMD_STORE_FILE          1101

/* VM controls */


#define ENCLAVE_CMD_CREATE_VM          1500
#define ENCLAVE_CMD_FREE_VM            1501
#define ENCLAVE_CMD_LAUNCH_VM          1502
#define ENCLAVE_CMD_STOP_VM            1503
#define ENCLAVE_CMD_PAUSE_VM           1504
#define ENCLAVE_CMD_CONTINUE_VM        1505
#define ENCLAVE_CMD_SIMULATE_VM        1506


#define ENCLAVE_CMD_VM_MOVE_CORE       1540
#define ENCLAVE_CMD_VM_DBG             1541

#define ENCLAVE_CMD_VM_CONS_CONNECT    1550
#define ENCLAVE_CMD_VM_CONS_DISCONNECT 1551
#define ENCLAVE_CMD_VM_CONS_KEYCODE    1552  




#define HOBBES_CMD_APP_LAUNCH          2000
#define HOBBES_CMD_APP_KILL            2001

#define HOBBES_CMD_VM_LAUNCH           2050
#define HOBBES_CMD_VM_DESTROY          2051

#define HOBBES_CMD_SHUTDOWN            2100


/* General Commands */
#define HOBBES_CMD_PING                3000

#define HOBBES_CMD_FILE_OPEN           4000
#define HOBBES_CMD_FILE_CLOSE          4001
#define HOBBES_CMD_FILE_READ           4002
#define HOBBES_CMD_FILE_WRITE          4003
#define HOBBES_CMD_FILE_SEEK           4004
#define HOBBES_CMD_FILE_STAT           4005
#define HOBBES_CMD_FILE_FSTAT          4006



#ifdef __cplusplus
}
#endif


#endif
