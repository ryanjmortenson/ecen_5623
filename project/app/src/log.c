/** @file log.c
*
* @brief Holds functions used for logging
*
*/

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "log.h"

// TODO: Make this work on both Linux and Windows for cross compilation
#define PATH_SEPARATOR "/"

// String log levels
static char * p_log_level_str[] = {
  "HIGH",
  "MEDIUM",
  "LOW",
  "ERROR",
  "FATAL"
};

#ifdef COLOR_LOGS
// String log level colors
static char * p_log_color_str[] = {
  "\e[1;96m", // High=CYAN
  "\e[1;93m", // Medium=YELLOW
  "\e[0;97m", // Low=WHITE
  "\e[1;91m", // Error=RED
  "\e[1;95m", // Fatal=PURPLE
};

// Format for color
#define LOG_COLOR_FMT   "%s%-7s %-10s in [%10s] line %4u: "

#else
// Format for non-color logs
#define LOG_FMT         "%-7s %-10s in [%10s] line %4u: "

#endif /* COLOR_LOGS */


// Logging mutex so log messages don't get interleaved
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/*!
* @brief Get the file basename in a OS that uses "/" for the separator
* @param[in] p_filename pointer to the file name
* @param[in] p_path_separator pointer to the path separator
* @return pointer to the basename
*/
static inline char * get_basename(char * p_filename, char * p_path_seperator)
{
  uint8_t found_last_occurence = 0;

  // Loop of string looking for instances of p_path_separator until
  // the last instance is found.  Use that as basename.
  while (!found_last_occurence)
  {
    char * p_temp = strstr(p_filename, p_path_seperator);
    if (p_temp != NULL)
    {
      p_filename = p_temp + 1;
    }
    else
    {
      found_last_occurence = 1;
    }
  }

  // Return a pointer to the file name
  return p_filename;
} // get_basename()

void log_level
(
  log_level_t level,
  char * p_filename,
  const char * p_function,
  uint32_t line_no,
  ...
)
{
  va_list printf_args;
  char * fmt;

  // Point to the last argument where the variadic arguments start
  va_start(printf_args, line_no);

  // Get the first argument which will be the format for the printf statement
  fmt = va_arg(printf_args, char *);

  // Lock the mutex so multiple calls to logging don't get interleaved
  pthread_mutex_lock(&log_mutex);
#ifdef COLOR_LOGS
  // Print header in color
  printf(LOG_COLOR_FMT,
         p_log_color_str[level],
         p_log_level_str[level],
         get_basename(p_filename, PATH_SEPARATOR),
         p_function,
         line_no);
#else
  // Print the header without color
  printf(LOG_FMT,
         p_log_level_str[level],
         get_basename(p_filename, PATH_SEPARATOR),
         p_function,
         line_no);
#endif /* COLOR_LOGS */
  // Print the statement provided in the ... variadic parameter
  vprintf(fmt, printf_args);

#ifdef COLOR_LOGS
  // Print the ending color and newline
  printf("\e[0m\n");
#else
  printf("\n");
#endif /* COLOR_LOGS */
  // Unlock mutex
  pthread_mutex_unlock(&log_mutex);
  va_end(printf_args);
} // log_level()
