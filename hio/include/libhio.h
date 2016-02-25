/*
 * libhio application interface
 * (c) Brian Kocoloski, 2016
 */

#ifndef __LIBHIO_H__
#define __LIBHIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdarg.h>

#include <pet_xml.h>

#include "libhio_types.h"
#include "libhio_error_codes.h"


#define HIO_CMD_CODE (uint64_t)0xf210

/* libhio_stub functions */
int
libhio_stub_init(int    * argc,
                 char *** argv);

void
libhio_stub_deinit(void);

int
libhio_event_loop(void);

int
libhio_register_cb(uint64_t cmd_code,
                   hio_cb_t cb);

#define libhio_register_stub_fn(cmd_code, cb) \
    libhio_register_cb(cmd_code, __##cb)
/* END libhio_stub functions */

/* libhio_client functions */

/* Use if a single process will make calls for all ranks in the app.
 * Ranks will need to be specified when issuing hio function calls
 */
int
libhio_client_init_app(char * hcq_name);

/* Use if all processes will issue their own hio function calls. Rank
 * need not be specified during function calls
 */
int
libhio_client_init(char   * hcq_name,
                   uint32_t hio_rank);


void
libhio_client_deinit(void);
/* END libhio_client functions */




/*
 * Convenience macros for defining stub functions
 */
#define DECLARE_TYPE(t, i) t _t##i
#define DECLARE_ARG(t, i)  DECLARE_TYPE(t, i)
#define DECLARE_VAR(t, i)  DECLARE_TYPE(t, i) = (t)args[i]

#define CHECK_TYPE(t)\
    if (sizeof(t) > sizeof(hio_arg_t))\
        return -HIO_INVALID_TYPE;

#define CHECK_TYPE1(t0, t1, t2, t3, t4, t5) \
    CHECK_TYPE(t0);

#define CHECK_TYPE2(t0, t1, t2, t3, t4, t5) \
    CHECK_TYPE1(t0, t1, t2, t3, t4, t5); CHECK_TYPE(t1);

#define CHECK_TYPE3(t0, t1, t2, t3, t4, t5) \
    CHECK_TYPE2(t0, t1, t2, t3, t4, t5); CHECK_TYPE(t2);

#define CHECK_TYPE4(t0, t1, t2, t3, t4, t5) \
    CHECK_TYPE3(t0, t1, t2, t3, t4, t5); CHECK_TYPE(t3);

#define CHECK_TYPE5(t0, t1, t2, t3, t4, t5) \
    CHECK_TYPE4(t0, t1, t2, t3, t4, t5); CHECK_TYPE(t4);

#define CHECK_TYPE6(t0, t1, t2, t3, t4, t5) \
    CHECK_TYPE5(t0, t1, t2, t3, t4, t5); CHECK_TYPE(t5);

#define DECLARE_VAR1(t0, t1, t2, t3, t4, t5) \
    DECLARE_VAR(t0, 0);

#define DECLARE_VAR2(t0, t1, t2, t3, t4, t5) \
    DECLARE_VAR1(t0, t1, t2, t3, t4, t5); DECLARE_VAR(t1, 1);

#define DECLARE_VAR3(t0, t1, t2, t3, t4, t5) \
    DECLARE_VAR2(t0, t1, t2, t3, t4, t5); DECLARE_VAR(t2, 2);

#define DECLARE_VAR4(t0, t1, t2, t3, t4, t5) \
    DECLARE_VAR3(t0, t1, t2, t3, t4, t5); DECLARE_VAR(t3, 3);

#define DECLARE_VAR5(t0, t1, t2, t3, t4, t5) \
    DECLARE_VAR4(t0, t1, t2, t3, t4, t5); DECLARE_VAR(t4, 4);
    
#define DECLARE_VAR6(t0, t1, t2, t3, t4, t5) \
    DECLARE_VAR5(t0, t1, t2, t3, t4, t5); DECLARE_VAR(t5, 5);

#define DECLARE_ARG1(t0, t1, t2, t3, t4, t5) \
    DECLARE_ARG(t0, 0)

#define DECLARE_ARG2(t0, t1, t2, t3, t4, t5) \
    DECLARE_ARG1(t0, t1, t2, t3, t4, t5), DECLARE_ARG(t1, 1)

#define DECLARE_ARG3(t0, t1, t2, t3, t4, t5) \
    DECLARE_ARG2(t0, t1, t2, t3, t4, t5), DECLARE_ARG(t2, 2)

#define DECLARE_ARG4(t0, t1, t2, t3, t4, t5) \
    DECLARE_ARG3(t0, t1, t2, t3, t4, t5), DECLARE_ARG(t3, 3)

#define DECLARE_ARG5(t0, t1, t2, t3, t4, t5) \
    DECLARE_ARG4(t0, t1, t2, t3, t4, t5), DECLARE_ARG(t4, 4)

#define DECLARE_ARG6(t0, t1, t2, t3, t4, t5) \
    DECLARE_ARG5(t0, t1, t2, t3, t4, t5), DECLARE_ARG(t5, 5)



#define LIBHIO_STUB_HDR(fn, cnt, ret_type, t0, t1, t2, t3, t4, t5) \
static int __##fn(uint32_t         argc,\
                  hio_arg_t      * args,\
                  hio_ret_t      * hio_ret,\
                  hio_segment_t ** seg_list,\
                  uint32_t       * nr_segs)\
{\
    *seg_list = NULL;\
    *nr_segs  = 0;\
    \
    if (argc != cnt)\
        return -HIO_INVALID_ARGC;\
    \
    CHECK_TYPE##cnt(t0, t1, t2, t3, t4, t5);\
    DECLARE_VAR##cnt(t0, t1, t2, t3, t4, t5);
#define LIBHIO_STUB_FTR }

#define LIBHIO_STUB1(fn, ret_type, t0)\
LIBHIO_STUB_HDR(fn, 1, ret_type, t0, 0, 0, 0, 0, 0) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_STUB_FTR

#define LIBHIO_STUB2(fn, ret_type, t0, t1)\
LIBHIO_STUB_HDR(fn, 2, ret_type, t0, t1, 0, 0, 0, 0) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, _t1, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_STUB_FTR

#define LIBHIO_STUB3(fn, ret_type, t0, t1, t2)\
LIBHIO_STUB_HDR(fn, 3, ret_type, t0, t1, t2, 0, 0, 0) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, _t1, _t2, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_STUB_FTR

#define LIBHIO_STUB4(fn, ret_type, t0, t1, t2, t3)\
LIBHIO_STUB_HDR(fn, 4, ret_type, t0, t1, t2, t3, 0, 0) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, _t1, _t2, _t3, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_STUB_FTR

#define LIBHIO_STUB5(fn, ret_type, t0, t1, t2, t3, t4)\
LIBHIO_STUB_HDR(fn, 5, ret_type, t0, t1, t2, t3, t4, 0) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, _t1, _t2, _t3, _t4, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_STUB_FTR

#define LIBHIO_STUB6(fn, ret_type, t0, t1, t2, t3, t4, t5)\
LIBHIO_STUB_HDR(fn, 6, ret_type, t0, t1, t2, t3, t4, t5) \
    *hio_ret = (hio_ret_t)(ret_type)fn(_t0, _t1, _t2, _t3, _t4, _t5, seg_list, nr_segs);\
    return HIO_SUCCESS;\
LIBHIO_STUB_FTR


static inline int
build_argc(pet_xml_t hio_xml,
           uint32_t  argc)
{
    char tmp_str[64] = {0};

    snprintf(tmp_str, 64, "%u", argc);
    if (pet_xml_add_val(hio_xml, "argc", tmp_str))
        return -HIO_NO_XML;

    return HIO_SUCCESS;
}

static inline int
build_arg(pet_xml_t hio_xml,
          hio_arg_t arg_val)
{
    pet_xml_t arg         = PET_INVALID_XML;
    char      tmp_str[64] = {0};

    arg = pet_xml_add_subtree_tail(hio_xml, "arg");
    if (arg == PET_INVALID_XML)
        return -HIO_NO_XML;

    snprintf(tmp_str, 64, "%li", arg_val);
    if (pet_xml_add_val(arg, "val", tmp_str)) {
        pet_xml_del_subtree(arg);
        return -HIO_NO_XML;
    }

    return HIO_SUCCESS;
}

#define CHECK_STATUS(s) \
    if (s != HIO_SUCCESS) {\
        pet_xml_free(hio_xml);\
        return s;\
    }

#define BUILD_ARG(hio_xml, _t)\
{\
    int status = build_arg(hio_xml, (hio_arg_t)_t); CHECK_STATUS(status);\
}

#define BUILD_ARGS1(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5) \
    BUILD_ARG(hio_xml, _t0);

#define BUILD_ARGS2(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5) \
    BUILD_ARGS1(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5); BUILD_ARG(hio_xml, _t1);

#define BUILD_ARGS3(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5) \
    BUILD_ARGS2(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5); BUILD_ARG(hio_xml, _t2);

#define BUILD_ARGS4(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5) \
    BUILD_ARGS3(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5); BUILD_ARG(hio_xml, _t3);

#define BUILD_ARGS5(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5) \
    BUILD_ARGS4(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5); BUILD_ARG(hio_xml, _t4);

#define BUILD_ARGS6(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5) \
    BUILD_ARGS5(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5); BUILD_ARG(hio_xml, _t5);


#define LIBHIO_CLIENT_COMMON(fn, cnt, cmd, ret_type, t0, t1, t2, t3, t4, t5)\
static ret_type fn(DECLARE_ARG##cnt(t0, t1, t2, t3, t4, t5))\
{\
    pet_xml_t hio_xml = PET_INVALID_XML;\
    hio_ret_t hio_ret = HIO_ERROR;\
    int       status  = 0;\
    \
    CHECK_TYPE(ret_type);\
    CHECK_TYPE##cnt(t0, t1, t2, t3, t4, t5);\
    \
    hio_xml = pet_xml_new_tree("hio");\
    if (hio_xml == PET_INVALID_XML)\
        return HIO_NO_XML;\
    \
    status = build_argc(hio_xml, cnt); CHECK_STATUS(status);\
    BUILD_ARGS##cnt(hio_xml, _t0, _t1, _t2, _t3, _t4, _t5);

#define LIBHIO_CLIENT_HDR(fn, cnt, cmd, ret_type, t0, t1, t2, t3, t4, t5)\
LIBHIO_CLIENT_COMMON(fn, cnt, cmd, ret_type, t0, t1, t2, t3, t4, t5)\
    status = libhio_client_call_stub_fn(cmd, hio_xml, &hio_ret); CHECK_STATUS(status);\
    pet_xml_free(hio_xml);\
    return (ret_type)hio_ret;

#define LIBHIO_CLIENT_HDR_APP(fn, cnt, cmd, rank, ret_type, t0, t1, t2, t3, t4, t5)\
LIBHIO_CLIENT_COMMON(fn, cnt, cmd, ret_type, t0, t1, t2, t3, t4, t5)\
    status = libhio_client_call_rank_stub_fn(cmd, rank, hio_xml, &hio_ret); CHECK_STATUS(status);\
    pet_xml_free(hio_xml);\
    return (ret_type)hio_ret;

#define LIBHIO_CLIENT_FTR }

#define LIBHIO_CLIENT1(fn, cmd, ret_type, t0)\
    LIBHIO_CLIENT_HDR(fn, 1, cmd, ret_type, t0, 0, 0, 0, 0, 0)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT2(fn, cmd, ret_type, t0, t1)\
    LIBHIO_CLIENT_HDR(fn, 2, cmd, ret_type, t0, t1, 0, 0, 0, 0)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT3(fn, cmd, ret_type, t0, t1, t2)\
    LIBHIO_CLIENT_HDR(fn, 3, cmd, ret_type, t0, t1, t2, 0, 0, 0)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT4(fn, cmd, ret_type, t0, t1, t2, t3)\
    LIBHIO_CLIENT_HDR(fn, 4, cmd, ret_type, t0, t1, t2, t3, 0, 0)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT5(fn, cmd, ret_type, t0, t1, t2, t3, t4)\
    LIBHIO_CLIENT_HDR(fn, 5, cmd, ret_type, t0, t1, t2, t3, t4, 0)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT6(fn, cmd, ret_type, t0, t1, t2, t3, t4, t5)\
    LIBHIO_CLIENT_HDR(fn, 6, cmd, ret_type, t0, t1, t2, t3, t4, t5)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT1_APP(fn, cmd, rank, ret_type, t0)\
    LIBHIO_CLIENT_HDR_APP(fn, 1, cmd, rank, ret_type, t0, 0, 0, 0, 0, 0)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT2_APP(fn, cmd, rank, ret_type, t0, t1)\
    LIBHIO_CLIENT_HDR_APP(fn, 2, cmd, rank, ret_type, t0, t1, 0, 0, 0, 0)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT3_APP(fn, cmd, rank, ret_type, t0, t1, t2)\
    LIBHIO_CLIENT_HDR_APP(fn, 3, cmd, rank, ret_type, t0, t1, t2, 0, 0, 0)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT4_APP(fn, cmd, rank, ret_type, t0, t1, t2, t3)\
    LIBHIO_CLIENT_HDR_APP(fn, 4, cmd, rank, ret_type, t0, t1, t2, t3, 0, 0)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT5_APP(fn, cmd, rank, ret_type, t0, t1, t2, t3, t4)\
    LIBHIO_CLIENT_HDR_APP(fn, 5, cmd, rank, ret_type, t0, t1, t2, t3, t4, 0)\
    LIBHIO_CLIENT_FTR

#define LIBHIO_CLIENT6_APP(fn, cmd, rank, ret_type, t0, t1, t2, t3, t4, t5)\
    LIBHIO_CLIENT_HDR_APP(fn, 6, cmd, rank, ret_type, t0, t1, t2, t3, t4, t5)\
    LIBHIO_CLIENT_FTR



#ifdef __cplusplus
}
#endif

#endif /* __LIBHIO_MACROS_H__ */
