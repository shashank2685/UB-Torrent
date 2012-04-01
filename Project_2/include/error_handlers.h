/*
 * *****************************************************************************
 * file name : errorHandlers.h
 * author    : Hung Q. Ngo (hungngo@cse.buffalo.edu)
 * description : 
 *  - for the error handling routines
 *  - These routine allow formatting of params like printf does
 * *****************************************************************************
 */

#ifndef _ERROR_HANDLERS_H
#define _ERROR_HANDLERS_H

#define OUTPUT_STREAM stdout
#define ERROR_STREAM  stdout

/*
 * set this to '1' for debugging mode. 
 * the function 'note' shall print things to screen when DEBUG is 1
 * turn it off (i.e. 0) when you think your program works
 */
#define DEBUG 1

#ifdef DBG 
    #define ENTER_FUNCTION printf("Entering function %s: %s\n", __FILE__,__FUNCTION__);
    #define LEAVE_FUNCTION printf("Leaving function %s: %s\n", __FILE__,__FUNCTION__);
#else
    #define ENTER_FUNCTION
    #define LEAVE_FUNCTION
#endif


extern int h_errno;    /* defined by BIND for DNS errors */

/*
 * -----------------------------------------------------------------------------
 * our error handling function prototypes
 * -----------------------------------------------------------------------------
 */
void app_error(const char* msg, ...); 
void posix_error(int, const char* msg, ...); 
void dns_error(const char* msg, ...); 
void sys_error(const char* msg, ...); 
void report_error(const char* msg, ...); 
void note(const char* msg, ...);
void warning(const char* msg, ...);

#endif /* _ERROR_HANDLERS_H */
