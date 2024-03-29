/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */


#ifndef __HOBBES_NOTIFIER_H__
#define __HOBBES_NOTIFIER_H__

#ifdef __cplusplus
extern "C" {
#endif
    


#define HNOTIF_EVT_ENCLAVE	(0x0000000000000001ULL)
#define HNOTIF_EVT_RESOURCE	(0x0000000000000002ULL)
#define HNOTIF_EVT_APPLICATION	(0x0000000000000004ULL)

#define HNOTIF_UNUSED_FLAGS	(0xfffffffffffffff8ULL)

typedef void * hnotif_t;

hnotif_t hnotif_create (uint64_t evt_mask);
void     hnotif_free   (hnotif_t notifier);


int hnotif_get_fd(hnotif_t notifier);

int hnotif_signal(uint64_t evt_mask);
int hnotif_ack(int fd);






#ifdef __cplusplus
}
#endif

#endif
