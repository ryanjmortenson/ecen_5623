/** @file log.h
*
* @brief Defines items used in logging
*
*/

#ifndef __LOG_H__
#define __LOG_H__

#include <cstdio>

// Logging level enumerations
typedef enum {
  LOG_LEVEL_HIGH,
  LOG_LEVEL_MEDIUM,
  LOG_LEVEL_LOW,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_FATAL
} log_level_t;

/*!
* @brief Initializes the syslog
*/
void log_init();

/*!
* @brief Destroy syslog
*/
void log_destroy();

/*!
* @brief Log at a certain level
* @param[in] level logging level for this statement
* @param[in] p_filename pointer to the file name
* @param[in] p_function pointer to the function name
* @param[in] line_no line number in file
* @param[in] ... variadic arguments for printf
* @return pointer to the basename
*/
void log_level
(
  log_level_t level,
  char * p_filename,
  const char * p_function,
  uint32_t line_no,
  ...
);

#define LOG(level, ...) log_level(level,                 \
                                  (char *)__FILE__,      \
                                  __FUNCTION__,          \
                                  __LINE__,              \
                                  __VA_ARGS__)

// Different log levels which can be turned on/off by setting LOG_LEVEL
#if LOG_LEVEL > 0
#define LOG_HIGH(...) LOG(LOG_LEVEL_HIGH, __VA_ARGS__)
#else
#define LOG_HIGH(...)
#endif /* LOG_LEVEL > 0 */

#if LOG_LEVEL > 1
#define LOG_MED(...)  LOG(LOG_LEVEL_MEDIUM, __VA_ARGS__)
#else
#define LOG_MED(...)
#endif  /* LOG_LEVEL > 1 */

#if LOG_LEVEL > 2
#define LOG_LOW(...)  LOG(LOG_LEVEL_LOW, __VA_ARGS__)
#else
#define LOG_LOW(...)
#endif  /* LOG_LEVEL > 2 */

// Function entry log.  This should be placed at the beginning of each function
#define FUNC_ENTRY LOG_MED("Entering %s()", __FUNCTION__)

// Error and Fatal definitions.  Fatal should be used sparingly and
// mostly debugging
#define LOG_ERROR(...) LOG(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_FATAL(...) LOG(LOG_LEVEL_FATAL, __VA_ARGS__)

#endif /* __LOG_H__ */
