/*
 * *****************************************************************************
 * file name   : include/network_util.h
 * author      : Hung Q. Ngo (hungngo@cse.buffalo.edu)
 * description : for basic network programming needs
 * *****************************************************************************
 */

#ifndef _NETWORK_UTIL_H
#define _NETWORK_UTIL_H

/*
 * -----------------------------------------------------------------------------
 * misc. macros and constants
 * -----------------------------------------------------------------------------
 */

/*
 * The following could be derived from SOMAXCONN in <sys/socket.h>, but many
 * kernels still #define it as 5, while actually supporting many more
 */
#define LISTENQ      1024     /* 2nd argument to listen()          */

/* to shorten all the type casts of pointer arguments */
#define SA           struct sockaddr

/* other constants */
#define MAXSOCKADDR  128      /* max socket address structure size */

/*
 * -----------------------------------------------------------------------------
 * prototypes for our utility functions
 * -----------------------------------------------------------------------------
 */
ssize_t	 readn(int, void*, size_t);
ssize_t	 writen(int, const void*, size_t);
ssize_t	 readline(int, void*, size_t);

#endif /* _NETWORK_UTIL_H */
