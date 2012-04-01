/*
 * *****************************************************************************
 * file name   : network_util.c
 * author      : Hung Q. Ngo (hungngo@cse.buffalo.edu)
 * description : implementations of basic networking functions
 *               modified from Stevens' code
 * *****************************************************************************
 */

/*
 * -----------------------------------------------------------------------------
 * headers related to networking functions
 * -----------------------------------------------------------------------------
 */
#include  <sys/socket.h>  /* basic socket definitions               */
#include  <netinet/in.h>  /* sockaddr_in{} and other Internet defns */
#include  <netdb.h>       /* name and address conversion            */
#include  <arpa/inet.h>   /* inet_pton/ntop                         */


#include "common_headers.h"
#include "network_util.h"
#include "error_handlers.h"

#define MAXLINE 2000


/*
 * ----------------------------------------------------------------------------
 * my_read:
 *   internal function, reads upto MAXLINE characters at a time, and then
 *     return them one at a time.
 *   this is much more efficient than 'read'ing one byte at a time, but the 
 *     price is that it's not "reentrant" or "thread-safe", due to the use
 *     of static variables
 * ----------------------------------------------------------------------------
 */
static ssize_t my_read(int fd, char* ptr)
{
  static int   read_count = 0;
  static char* read_ptr;
  static char  read_buf[MAXLINE];
  int          got_signal;
  
  got_signal = 0;
  if (read_count <= 0) {
  again:
    if ( (read_count = read(fd, read_buf, sizeof(read_buf))) < 0) {
      if (errno == EINTR) {
	goto again;  /* Dijkstra hates this, but he's not reading our code */
      } else {
	return(-1);
      }
    } else if (read_count == 0) {
      return(0);
    }
    read_ptr = read_buf;
  }
  
  read_count--;
  *ptr = *read_ptr++;
  return(1);
}

/*
 * ----------------------------------------------------------------------------
 * readline:
 *   read upto '\n' or maxlen bytes from 'fd' into 'vptr'
 *   returns number of bytes read or -1 on error
 * ----------------------------------------------------------------------------
 */
ssize_t readline(int fd, void *vptr, size_t maxlen)
{
  int  n, rc;
  char c, *ptr;

  ptr = vptr;
  for (n = 1; n < maxlen; n++) {
    if ( (rc = my_read(fd, &c)) == 1) {
      *ptr++ = c;
      if (c == '\n') {
	break;     /* newline is stored, like fgets() */
      }
    } else if (rc == 0) {
      if (n == 1) {
	return(0); /* EOF, no data read       */
      } else {
	break;     /* EOF, some data was read */
      }
    } else {
      return(-1);  /* error, errno set by read()  */
    }
  }
  
  *ptr = 0;        /* null terminate like fgets() */
  return(n);
}

/*
 * ----------------------------------------------------------------------------
 * readn:
 *   read 'n' bytes or upto EOF from descriptor 'fd' into 'vptr'
 *   returns number of bytes read or -1 on error
 *   our program will be blocked waiting for n bytes to be available on fd
 *
 * fd:   the file descriptor (socket) we're reading from
 * vptr: address of memory space to put read data
 * n:    number of bytes to be read
 * ----------------------------------------------------------------------------
 */ 
ssize_t readn(int fd, void* vptr, size_t n)
{
  size_t  nleft;
  ssize_t nread;
  char*   ptr;
  
  ptr = vptr;
  nleft = n;
  while (nleft > 0) {           /* keep reading upto n bytes     */
    if ( (nread = read(fd, ptr, nleft)) < 0) {
      if (errno == EINTR) {     /* got interrupted by a signal ? */
        nread = 0;              /* try calling read() again      */
      } else {
        return(-1);
      }
    } else if (nread == 0) {
      break;                    /* EOF */
    }
    nleft -= nread;
    ptr   += nread;
  }

  return(n - nleft);            /* return >= 0 */
}

/*
 * ----------------------------------------------------------------------------
 * writen:
 *   write 'n' bytes from 'vptr' to descriptor 'fd'
 *   returns number of bytes written or -1 on error
 * ----------------------------------------------------------------------------
 */
ssize_t writen(int fd, const void* vptr, size_t n)
{
  size_t      nleft;
  ssize_t     nwritten;
  const char* ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
      if (errno == EINTR) { /* interrupted by a signal ? */
	nwritten = 0;       /* try call write() again    */
      } else {
	return(-1);
      }
    }
    nleft -= nwritten;
    ptr   += nwritten;
  }
  return(n);
}
