/*
 * *****************************************************************************
 * file name   : common_headers.h
 * description : 
 *  - headers and definitions that most of your program files would need
 * *****************************************************************************
 */

#ifndef _COMMON_HEADERS_H
#define _COMMON_HEADERS_H

/*
 * -----------------------------------------------------------------------------
 * standard C programming headers, basic Unix system programming headers
 * -----------------------------------------------------------------------------
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	<unistd.h>      /* basic Unix system programming          */
#include	<sys/types.h>	/* basic system data types                */
#include	<sys/time.h>	/* timeval{} for select()                 */
#include	<sys/wait.h>    /* 'wait' and 'waitpid'                   */
#include	<sys/stat.h>    /* 'wait' and 'waitpid'                   */
#include    <sys/select.h>  /* IO multiplexing                        */
#include	<errno.h>       /* error handling                         */
#include	<signal.h>      /* signal handling                        */
#include    <pthread.h>
#include    <stdint.h>
#include    <fcntl.h>
#include    <ctype.h>
#define	min(a,b)     ((a) < (b) ? (a) : (b))
#define	max(a,b)     ((a) > (b) ? (a) : (b))


#endif /* _COMMON_HEADERS_H */
