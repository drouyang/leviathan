/* 
 * HIO Ringbufffer
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */
#include "hio.h"

int hio_engine_init(struct hio_engine *hio_engine) {
    return 0;
}


int 
insert_stub(struct hio_engine *hio_engine, 
        int key, struct hio_stub *stub) {
    if (hio_engine->stub_lookup_table[key] != NULL) {
        printk(KERN_ERR "Failed to insert duplicated stub key %d\n", key);
        return -1;
    }
    hio_engine->stub_lookup_table[key] = stub;
    return 0;
}


int
remove_stub(struct hio_engine *hio_engine, int key) {
    int ret = 0;
    if (hio_engine->stub_lookup_table[key] != NULL) {
        printk(KERN_WARNING "Trying to remove a non-existing stub, key=%d\n", key);
        ret = -1;
    }
    hio_engine->stub_lookup_table[key] = NULL;
    return ret;
}


struct hio_stub * 
lookup_stub(struct hio_engine *hio_engine, int key) {
    return hio_engine->stub_lookup_table[key];
}
