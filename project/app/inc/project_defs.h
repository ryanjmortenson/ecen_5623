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


// Check value not equal, print errno, and exit
#define NOT_EQ_EXIT_E(res, func, val)                           \
  if((res = func) != val)                                       \
  {                                                             \
    LOG_ERROR(#func " failed with error: %s", strerror(errno)); \
    exit(1);                                                    \
  }

// Check value not equal, print errno, and return failure
#define NOT_EQ_RET_E(res, func, val, ret)                       \
  if((res = func) != val)                                       \
  {                                                             \
    LOG_ERROR(#func " failed with error: %s", strerror(errno)); \
    return ret;                                                 \
  }

// Check value equal, print errno, and return failure
#define EQ_RET_E(res, func, val, ret)                           \
  if((res = func) == val)                                       \
  {                                                             \
    LOG_ERROR(#func " failed with error: %s", strerror(errno)); \
    return ret;                                                 \
  }

// Check value equal, print errno, and return failure
#define EQ_EXIT_E(res, func, val)                               \
  if((res = func) == val)                                       \
  {                                                             \
    LOG_ERROR(#func " failed with error: %s", strerror(errno)); \
    exit(1);                                                    \
  }

#define PT_NOT_EQ_RET(res, func, val, ret)                      \
  if((res = func) != val)                                       \
  {                                                             \
    LOG_ERROR(#func " failed with error: %s", strerror(res));   \
    return ret;                                                 \
  }

#define PT_EQ_RET(res, func, val, ret)                          \
  if((res = func) == val)                                       \
  {                                                             \
    LOG_ERROR(#func " failed with error: %s", strerror(res));   \
    return ret;                                                 \
  }

#define PT_NOT_EQ_EXIT(res, func, val)                          \
  if((res = func) != val)                                       \
  {                                                             \
    LOG_ERROR(#func " failed with error: %s", strerror(res));   \
    exit(res);                                                  \
  }

#endif /* __PROJECT_DEFS_H__ */
