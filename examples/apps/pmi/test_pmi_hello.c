#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <pmi.h>

typedef enum {
    PMI_UNINITIALIZED = 0,
    NORMAL_INIT_WITH_PM = 2,
} PMIState;

int
main(int argc, char *argv[])
{
    int initialized, rank=-1, size=-1, usize=-1;
    int i, max_name_len=-1, max_key_len=-1, max_val_len=-1;
    char *name, *key, *val;

    if (PMI_Initialized(&initialized) != PMI_SUCCESS)
        return -1;

    if (initialized == PMI_UNINITIALIZED) {
        if (PMI_Init(&initialized) != PMI_SUCCESS)
            return -1;
    }

    if (PMI_Get_rank(&rank) != PMI_SUCCESS)
        return -1;

    if (PMI_Get_size(&size) != PMI_SUCCESS)
        return -1;

    if (PMI_Get_universe_size(&usize) != PMI_SUCCESS)
        return -1;

    printf("%d: Hello from rank %d, world size=%d, universe size=%d.\n",
           rank, rank, size, usize);

    if (PMI_KVS_Get_name_length_max(&max_name_len) != PMI_SUCCESS)
        return -1;

    if (PMI_KVS_Get_key_length_max(&max_key_len) != PMI_SUCCESS)
        return -1;

    if (PMI_KVS_Get_value_length_max(&max_val_len) != PMI_SUCCESS)
        return -1;

    if ((name = malloc(max_name_len)) == NULL)
        return -1;

    if ((key = malloc(max_key_len)) == NULL)
        return -1;

    if ((val = malloc(max_val_len)) == NULL)
        return -1;

    if (PMI_KVS_Get_my_name(name, max_name_len) != PMI_SUCCESS)
        return -1;
   
    /* Put my information into the PMI keyval store */
    snprintf(key, max_key_len, "pmi_hello-%lu-test", (long unsigned) rank);
    snprintf(val, max_val_len, "%lu", (long unsigned) rank);

    printf("%d: PMI_KVS_Put(%s, %s)\n", rank, key, val);

    if (PMI_KVS_Put(name, key, val) != PMI_SUCCESS)
        return -1;

    if (PMI_KVS_Commit(name) != PMI_SUCCESS)
        return -1;		

    if (PMI_Barrier() != PMI_SUCCESS)
        return -1;

    /* Get everybody's info from the PMI keyval store and verify that it's correct */
    for (i = 0 ; i < size ; ++i) {
        snprintf(key, max_key_len, "pmi_hello-%lu-test", (long unsigned) i);

        if (PMI_KVS_Get(name, key, val, max_val_len) != PMI_SUCCESS)
            return -1;

        printf("%d: PMI_KVS_Get(%s) = %s\n", rank, key, val);

        if (i != strtol(val, NULL, 0)) {
            printf("%d: Error: Expected %d, got %d\n", rank, i, (int) strtol(val, NULL, 0));
            return -1;
        }
    }

    printf("%d: Calling PMI_Finalize()\n", rank);
    PMI_Finalize();

    printf("%d: Done.\n", rank);

    return 0;
}

/* vim:set expandtab: */
