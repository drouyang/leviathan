/*
 * Convenience macros for defining and invoking libhio functions
 * (c) Brian Kocoloski, 2016
 */

#ifndef __LIBHIO_MACROS_H__
#define __LIBHIO_MACROS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdarg.h>

#include <pet_log.h>

#include "libhio.h"
#include "libhio_error_codes.h"





#define DECLARE_ARG(t, i) t _t##i = (t)args[i]

#define CHECK_ARG(t) \
    if (sizeof(t) > sizeof(hio_arg_t)) {\
        ERROR("%s: Invalid arg type. Args must be %lu bytes or less in size\n",\
            __FUNCTION__, sizeof(hio_arg_t));\
        return -HIO_INVALID_ARG;\
    }\

#define DECLARE_AND_CHECK_ARGS1(t0, t1, t2, t3, t4, t5) \
    DECLARE_ARG(t0, 0); CHECK_ARG(t0);

#define DECLARE_AND_CHECK_ARGS2(t0, t1, t2, t3, t4, t5) \
    DECLARE_AND_CHECK_ARGS1(t0, t1, t2, t3, t4, t5); \
    DECLARE_ARG(t1, 1);

#define DECLARE_AND_CHECK_ARGS3(t0, t1, t2, t3, t4, t5) \
    DECLARE_AND_CHECK_ARGS2(t0, t1, t2, t3, t4, t5); \
    DECLARE_ARG(t2, 2);

#define DECLARE_AND_CHECK_ARGS4(t0, t1, t2, t3, t4, t5) \
    DECLARE_AND_CHECK_ARGS3(t0, t1, t2, t3, t4, t5); \
    DECLARE_ARG(t3, 3);

#define DECLARE_AND_CHECK_ARGS5(t0, t1, t2, t3, t4, t5) \
    DECLARE_AND_CHECK_ARGS4(t0, t1, t2, t3, t4, t5); \
    DECLARE_ARG(t4, 4);

#define DECLARE_AND_CHECK_ARGS6(t0, t1, t2, t3, t4, t5) \
    DECLARE_AND_CHECK_ARGS5(t0, t1, t2, t3, t4, t5); \
    DECLARE_ARG(t5, 5);
    

#define LIBHIO_FN_HDR(fn, cnt, ret_type, t0, t1, t2, t3, t4, t5) \
static int __libhio_##fn(uint32_t         argc,\
                         hio_arg_t      * args,\
                         hio_ret_t      * hio_ret,\
                         hio_segment_t ** seg_list,\
                         uint32_t       * nr_segs)\
{\
    *seg_list = NULL;\
    *nr_segs  = 0;\
    if (argc != cnt) {\
        ERROR("%s: Invalid argc (%d). Expected %d\n", __FUNCTION__, argc, cnt);\
        return -HIO_INVALID_ARGC;\
    }\
    DECLARE_AND_CHECK_ARGS##cnt(t0, t1, t2, t3, t4, t5);\

#define LIBHIO_FN_FTR }

#define LIBHIO_CB1(fn, ret_type, t0)\
LIBHIO_FN_HDR(fn, 1, ret_type, t0, 0, 0, 0, 0, 0) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_FN_FTR

#define LIBHIO_CB2(fn, ret_type, t0, t1)\
LIBHIO_FN_HDR(fn, 2, ret_type, t0, t1, 0, 0, 0, 0) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, _t1, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_FN_FTR

#define LIBHIO_CB3(fn, ret_type, t0, t1, t2)\
LIBHIO_FN_HDR(fn, 3, ret_type, t0, t1, t2, 0, 0, 0) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, _t1, _t2, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_FN_FTR

#define LIBHIO_CB4(fn, ret_type, t0, t1, t2, t3)\
LIBHIO_FN_HDR(fn, 4, ret_type, t0, t1, t2, t3, 0, 0) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, _t1, _t2, _t3, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_FN_FTR

#define LIBHIO_CB5(fn, ret_type, t0, t1, t2, t3, t4)\
LIBHIO_FN_HDR(fn, 5, ret_type, t0, t1, t2, t3, t4, 0) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, _t1, _t2, _t3, _t4, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_FN_FTR

#define LIBHIO_CB6(fn, ret_type, t0, t1, t2, t3, t4, t5)\
LIBHIO_FN_HDR(fn, 6, ret_type, t0, t1, t2, t3, t4, t5) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, _t1, _t2, _t3, _t4, _t5, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_FN_FTR


#ifdef __cplusplus
}
#endif

#endif /* __LIBHIO_MACROS_H__ */
