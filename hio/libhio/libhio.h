/*
 * libhio application interface
 * (c) Brian Kocoloski, 2016
 */

#ifndef __LIBHIO_H__
#define __LIBHIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <xemem.h>
#include <hobbes_cmd_queue.h>



typedef struct hio_segment {
    xemem_segid_t segid;
    uint64_t      size;
    void        * vaddr;
} hio_segment_t;

typedef intptr_t hio_arg_t;
typedef intptr_t hio_ret_t;

typedef int32_t (*hio_cb_t)
        (uint32_t         argc,
         hio_arg_t      * args,
         hio_ret_t      * hio_ret,
         hio_segment_t ** seg_list,
         uint32_t       * nr_segs);

int
libhio_init(int    * argc,
            char *** argv);

void
libhio_deinit(void);

int
libhio_register_cb(uint64_t    cmd_code,
                   hio_cb_t    cb);

#define libhio_register_callback(cmd_code, cb) \
    libhio_register_cb(cmd_code, __libhio_##cb)


int
libhio_event_loop(void);


#ifdef __cplusplus
}
#endif

#endif /* __LIBHIO_H__ */
