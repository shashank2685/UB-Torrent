/*
 * *****************************************************************************
 * file name   : error_handlers.c
 * author      : Hung Q. Ngo (hungngo@cse.buffalo.edu)
 * description : implementations of error handling routines
 * *****************************************************************************
 */
#include <stdarg.h>
#include "common_headers.h"
#include "error_handlers.h"
#include "color.h"
#include <netdb.h>

/*
 * ----------------------------------------------------------------------------
 * posix_error
 *   handles DNS's error, note that exit(1) does the cleaning for us
 * ----------------------------------------------------------------------------
 */
void posix_error(int code, const char *formatted_msg, ...)
{
  va_list args;
  va_start(args, formatted_msg);
  vfprintf(ERROR_STREAM, formatted_msg, args);
  fprintf(ERROR_STREAM, " ==> %s\n", strerror(code));
  fflush(ERROR_STREAM);
  va_end(args);
  exit(1);
}


/*
 * ----------------------------------------------------------------------------
 * dns_error
 *   handles DNS's error, note that exit(1) does the cleaning for us
 * ----------------------------------------------------------------------------
 */ 
void dns_error(const char *formatted_msg, ...) 
{
  va_list args;
  va_start(args, formatted_msg);
  vfprintf(ERROR_STREAM, formatted_msg, args);
  fprintf(ERROR_STREAM, " DNS error: %s\n", strerror(h_errno));
  fflush(ERROR_STREAM);
  va_end(args);
  exit(1);
}

/*
 * ----------------------------------------------------------------------------
 * app_error
 *   handles application's error, note that exit(1) does the cleaning for us
 * ----------------------------------------------------------------------------
 */ 
void app_error(const char *formatted_msg, ...) 
{
  va_list args;
  va_start(args, formatted_msg);
  vfprintf(ERROR_STREAM, formatted_msg, args);
  fputc('\n', OUTPUT_STREAM);
  fflush(ERROR_STREAM);
  va_end(args);
  exit(1);
}

/*
 * ----------------------------------------------------------------------------
 * sys_error
 *   system call's errors
 * ----------------------------------------------------------------------------
 */
void sys_error(const char *formatted_msg, ...)
{
  va_list args;
  va_start(args, formatted_msg);
  vfprintf(ERROR_STREAM, formatted_msg, args);
  fprintf(ERROR_STREAM, " ERROR: %s\n", strerror(errno));
  fflush(ERROR_STREAM);
  va_end(args);
  exit(1);
}


/*
 * ----------------------------------------------------------------------------
 * report_error
 *   handles non-fatal error, reports the error and returns to the calling func.
 * ----------------------------------------------------------------------------
 */ 
void report_error(const char *formatted_msg, ...) 
{
  va_list args;
  va_start(args, formatted_msg);
  fprintf(ERROR_STREAM, "ERROR: ");
  vfprintf(ERROR_STREAM, formatted_msg, args);
  fprintf(ERROR_STREAM, " ==> %s\n", strerror(errno));
  fflush(ERROR_STREAM);
  va_end(args);
}

/*
 * ----------------------------------------------------------------------------
 * note:
 * for debugging purposes, prints out a "NOTE: ..." to OUTPUT_STREAM
 * ----------------------------------------------------------------------------
 */ 
void note(const char *formatted_msg, ...) 
{
  va_list args;
  va_start(args, formatted_msg);
  fprintf(OUTPUT_STREAM, "NOTE: ");
  vfprintf(OUTPUT_STREAM, formatted_msg, args);
  fputc('\n', OUTPUT_STREAM);
  fflush(OUTPUT_STREAM);
  va_end(args);
}

/*
 * ----------------------------------------------------------------------------
 * warning:
 * prints a warning, notice that this function is used when the message is NOT
 * an error. For definitions of 'errors', 'mistakes', 'warnings', etc.
 * consult a software engineering book 
 * ----------------------------------------------------------------------------
 */ 
void warning(const char *formatted_msg, ...) 
{
  va_list args;
  va_start(args, formatted_msg);
  fprintf(OUTPUT_STREAM, "WARNING: ");
  vfprintf(OUTPUT_STREAM, formatted_msg, args);
  fputc('\n', OUTPUT_STREAM);
  fflush(OUTPUT_STREAM);
  va_end(args);
}
