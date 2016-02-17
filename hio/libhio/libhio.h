/*
 * libhio application interface
 * (c) Brian Kocoloski, 2016
 */

#ifndef __LIBHIO_H__
#define __LIBHIO_H__

#ifdef __cplusplus
extern "C" {
#endif

int
libhio_init(int    * argc,
            char *** argv);

void
libhio_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __LIBHIO_C__ */
