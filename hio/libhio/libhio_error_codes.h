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
#define HIO_ERROR         -1

/* Specific codes */
#define HIO_INVALID_ARGC  100
#define HIO_INVALID_ARG   101




#ifdef __cplusplus
}
#endif

#endif /* __LIBHIO_ERROR_CODES_H__ */
