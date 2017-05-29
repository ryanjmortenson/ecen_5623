/** @file log.h
*
* @brief Defines items used in logging
*
*/

#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

// TODO: Make this work on both Linux and Windows for cross compilation
#define PATH_SEPERATOR "/"

// Set up logging based on whether color logs was requested
#ifdef COLOR_LOGS
#define LOG_FMT   "%s%-7s %-10s in [%10s] line %4u: "
#define HIGH      "\e[1;96m", "HIGH"
#define MEDIUM    "\e[1;93m", "MEDIUM"
#define LOW       "\e[0;97m", "LOW"
#define ERROR     "\e[1;91m", "ERROR"
#define FATAL     "\e[1;95m", "FATAL"
#define BODY(...) printf(__VA_ARGS__); printf("\e[0m\n")
#else
#define LOG_FMT   "%-7s %-10s in [%10s] line %4u: "
#define HIGH      "HIGH"
#define MEDIUM    "MEDIUM"
#define LOW       "LOW"
#define ERROR     "ERROR"
#define FATAL     "FATAL"
#define BODY(...) printf(__VA_ARGS__); printf("\n")
#endif

/*!
* @brief Get the file basename in a OS that uses "/" for the seperator
* @param[in] p_filename pointer to the file name
* @param[in] p_path_seperator pointer to the path sepearator
* @return pointer to the basename
*/
char * get_basename(char * p_filename, char * p_path_seperator);

#define HEADER(lvl) printf(LOG_FMT,                                \
                           lvl,                                    \
                           get_basename(__FILE__, PATH_SEPERATOR), \
                           __FUNCTION__,                           \
                           __LINE__)

// Different log levels which can be turned on/off by setting LOG_LEVEL
#if LOG_LEVEL > 0
#define LOG_HIGH(...) HEADER(HIGH); BODY(__VA_ARGS__)
#else
#define LOG_HIGH(...)
#endif // LOG_LEVEL > 0

#if LOG_LEVEL > 1
#define LOG_MED(...)  HEADER(MEDIUM); BODY(__VA_ARGS__)
#else
#define LOG_MED(...)
#endif // LOG_LEVEL > 1

#if LOG_LEVEL > 2
#define LOG_LOW(...)  HEADER(LOW); BODY(__VA_ARGS__)
#else
#define LOG_LOW(...)
#endif  // LOG_LEVEL > 2

// Function entry log.  This should be placed at the beginning of each function
#define FUNC_ENTRY LOG_LOW("Entering %s()", __FUNCTION__)

// Error and Fatal definitions.  Fatal should be used sparingly and mostly debugging
#define LOG_ERROR(...)  HEADER(ERROR); BODY(__VA_ARGS__)
#define LOG_FATAL(...)  HEADER(FATAL); BODY(__VA_ARGS__)

#endif /* __LOG_H__ */
