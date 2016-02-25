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
#define HIO_SUCCESS       0

/* Generic error occured in command processing */
#define HIO_ERROR         1


/* HCQ issue error */
#define HIO_HCQ_FAULT     2

/* Error in XML generation/parsgingn */
#define HIO_NO_XML        3
#define HIO_BAD_XML       4

/* Client errors */

/* Initialization errors */
#define HIO_NO_HCQ	      5
#define HIO_NO_RANK	      6
#define HIO_WRONG_MODE    7


/* Argument generation */
#define HIO_INVALID_ARGC  100
#define HIO_INVALID_TYPE  101




#ifdef __cplusplus
}
#endif

#endif /* __LIBHIO_ERROR_CODES_H__ */
