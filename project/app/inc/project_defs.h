/** @file project_defs.h
*
* @brief Holds common project definitions
*
*/

#ifndef __PROJECT_DEFS_H__
#define __PROJECT_DEFS_H__

typedef enum status {
  SUCCESS,
  FAILURE
} status_t;


// Check and exit macro to be used from the main function
#define CHECK_AND_EXIT(res, func, val)                          \
  if((res = func) != val)                                       \
  {                                                             \
    LOG_ERROR(#func " failed with error: %s", strerror(errno)); \
    exit(1);                                                    \
  }

// Check and return macro to be used from subroutines
#define CHECK_AND_RETURN(res, func, val) \
  if((res = func) != val)                                       \
  {                                                             \
    LOG_ERROR(#func " failed with error: %s", strerror(errno)); \
    return FAILURE;                                             \
  }

// Check and return macro to be used from subroutines
#define CHECK_AND_PRINT(res, func, val) \
  if((res = func) != val)                                       \
  {                                                             \
    LOG_ERROR(#func " failed with error: %s", strerror(errno)); \
  }

#endif /* __PROJECT_DEFS_H__ */
