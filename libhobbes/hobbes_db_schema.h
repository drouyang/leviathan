/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#ifndef __HOBBES_DB_SCHEMA_H__
#define __HOBBES_DB_SCHEMA_H__

#ifdef __cplusplus
extern "C" {
#endif


/* 
 * Database Record Type
 *    Each row in the database has an associated type from the list below
 */
#define HDB_REC_ENCLAVE               0
#define HDB_REC_APP                   1
#define HDB_REC_SEGMENT               2
#define HDB_REC_ENCLAVE_HDR           3
#define HDB_REC_APP_HDR               4
#define HDB_REC_XEMEM_HDR             5
#define HDB_REC_XEMEM_SEGMENT         6
#define HDB_REC_XEMEM_ATTACHMENT      7
#define HDB_REC_PMI_KEYVAL            8
#define HDB_REC_PMI_BARRIER           9
#define HDB_REC_SYS_HDR               10
#define HDB_REC_CPU                   11
#define HDB_REC_MEM                   12
#define HDB_REC_NOTIFIER              13

/*
 * Database Field definitions
 *     The column types for each row are specified below
 */

/* The first column of every row specifies the row type */
#define HDB_TYPE_FIELD                0 

/* Columns for enclave header */
#define HDB_ENCLAVE_HDR_NEXT          1
#define HDB_ENCLAVE_HDR_CNT           2

/* Columns for enclave records */
#define HDB_ENCLAVE_ID                1
#define HDB_ENCLAVE_TYPE              2
#define HDB_ENCLAVE_DEV_ID            3
#define HDB_ENCLAVE_STATE             4
#define HDB_ENCLAVE_NAME              5
#define HDB_ENCLAVE_CMDQ_ID           6
#define HDB_ENCLAVE_PARENT            7

/* Columns for XEMEM segment header */
#define HDB_SEGMENT_HDR_CNT           1

/* Columns for XEMEM segment records */
#define HDB_SEGMENT_SEGID             1
#define HDB_SEGMENT_NAME              2
#define HDB_SEGMENT_ENCLAVE           3
#define HDB_SEGMENT_APP               4

/* Columns for application header */
#define HDB_APP_HDR_NEXT              1
#define HDB_APP_HDR_CNT               2

/* Columns for application records */
#define HDB_APP_ID                    1
#define HDB_APP_NAME                  2
#define HDB_APP_STATE                 3
#define HDB_APP_ENCLAVE               4

/* Columns for PMI key value store records */
#define HDB_PMI_KVS_ENTRY_APPID       1
#define HDB_PMI_KVS_ENTRY_KVSNAME     2
#define HDB_PMI_KVS_ENTRY_KEY         3
#define HDB_PMI_KVS_ENTRY_VALUE       4


/* Columns for PMI barrier records */
#define HDB_PMI_BARRIER_APPID         1
#define HDB_PMI_BARRIER_COUNTER       2
#define HDB_PMI_BARRIER_SEGIDS        3

/* Columns for System Information */
#define HDB_SYS_HDR_CPU_CNT           1
#define HDB_SYS_HDR_NUMA_CNT          2
#define HDB_SYS_HDR_MEM_BLK_SIZE      3
#define HDB_SYS_HDR_MEM_BLK_CNT       4
#define HDB_SYS_HDR_MEM_FREE_BLK_CNT  5
#define HDB_SYS_HDR_MEM_FREE_LIST     6
#define HDB_SYS_HDR_MEM_BLK_LIST      7

/* Columns for memory resource records */
#define HDB_MEM_BASE_ADDR             1
#define HDB_MEM_BLK_SIZE              2
#define HDB_MEM_NUMA_NODE             3
#define HDB_MEM_STATE                 4
#define HDB_MEM_ENCLAVE_ID            5
#define HDB_MEM_APP_ID                6
#define HDB_MEM_NEXT_FREE             7
#define HDB_MEM_PREV_FREE             8
#define HDB_MEM_NEXT_BLK              9
#define HDB_MEM_PREV_BLK              10

/* Columns for CPU resource records */
#define HDB_CPU_ID                    1
#define HDB_CPU_NUMA_NODE             2
#define HDB_CPU_STATE                 3
#define HDB_CPU_ENCLAVE_ID            4

/* Columns notification events */
#define HDB_NOTIF_SEGID               1
#define HDB_NOTIF_EVT_MASK            2

#ifdef __cplusplus
}
#endif


#endif
