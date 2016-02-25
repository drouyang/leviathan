/*
 * libhio error codes
 * (c) Brian Kocoloski, 2016
 */

#ifndef __LIBHIO_ERROR_CODES_H__
#define __LIBHIO_ERROR_CODES_H__

#ifdef __cplusplus
extern "C" {
#endif


/* Command issued successfully */
#define HIO_SUCCESS         0

/* Generic errors */
#define HIO_SERVER_ERROR    1
#define HIO_CLIENT_ERROR    2

/* HCQ errors */
#define HIO_BAD_SERVER_HCQ  3
#define HIO_BAD_CLIENT_HCQ  4

/* Error in XML generation/parsing */
#define HIO_BAD_XML         5

/* Client mode */
#define HIO_WRONG_MODE      6

/* Argument generation */
#define HIO_INVALID_ARGC    7
#define HIO_INVALID_TYPE    8

/* Other */
#define HIO_BAD_RANK        9
#define HIO_RANK_BUSY       10
#define HIO_NO_STUB_CMD     11


static inline char *
hio_error_to_str(int32_t error_code)
{
    switch (error_code) {
        case HIO_SUCCESS:         return "HIO_SUCCESS";
        case -HIO_SERVER_ERROR:   return "HIO_SERVER_ERROR";
        case -HIO_CLIENT_ERROR:   return "HIO_CLIENT_ERROR";
        case -HIO_BAD_SERVER_HCQ: return "HIO_BAD_SERVER_HCQ";
        case -HIO_BAD_CLIENT_HCQ: return "HIO_BAD_CLIENT_HCQ";
        case -HIO_BAD_XML:        return "HIO_BAD_XML";
        case -HIO_WRONG_MODE:     return "HIO_WRONG_MODE";
        case -HIO_INVALID_ARGC:   return "HIO_INVALID_ARGC";
        case -HIO_INVALID_TYPE:   return "HIO_INVALID_TYPE";
        case -HIO_BAD_RANK:       return "HIO_BAD_RANK";
        case -HIO_RANK_BUSY:      return "HIO_RANK_BUSY";
        case -HIO_NO_STUB_CMD:    return "HIO_NO_STUB_CMD";
        default:                  return "UNKNOWN_ERROR_CODE";
    }
}


#ifdef __cplusplus
}
#endif

#endif /* __LIBHIO_ERROR_CODES_H__ */
