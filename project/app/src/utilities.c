/** @file template.c
*
* @brief A description of the module’s purpose.
*
*/

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>

#include "log.h"
#include "project_defs.h"
#include "utilities.h"

uint32_t get_uname(char * uname_str, uint32_t count)
{
  FUNC_ENTRY;
  CHECK_NULL(uname_str);
  struct utsname info;
  uint32_t res = 0;

  // Create and write the uname info
  EQ_RET_E(res, uname(&info), -1, FAILURE);
  res = snprintf(uname_str,
                 count,
                 "# uname: %s %s %s %s %s\n",
                 info.sysname,
                 info.nodename,
                 info.release,
                 info.version,
                 info.machine);

  return SUCCESS;
}

uint32_t get_timestamp(struct timespec * time, char * timestamp, uint32_t count)
{
  CHECK_NULL(time);
  CHECK_NULL(timestamp);

  // Create and write the timestamp
  snprintf(timestamp,
           count,
           "# Timestamp: %f\n",
           (double) time->tv_sec + (double) time->tv_nsec / 1000000000);

  return SUCCESS;
}

uint32_t create_dir(char * dir_name)
{
  FUNC_ENTRY;
  CHECK_NULL(dir_name);

  struct timespec dir_time;
  uint32_t res = 0;

  // Check for null pointer
  CHECK_NULL(dir_name);

  // Get the system time to append to directory to be created
  clock_gettime(CLOCK_REALTIME, &dir_time);

  // Log the newly create directory name
  LOG_LOW("Trying to create directory name: %s", dir_name);

  res = mkdir(dir_name, FILE_PERM);
  if (res == 0)
  {
    return SUCCESS;
  }
  else if (res == -1 && errno == EEXIST)
  {
    LOG_MED("%s directory already exists", dir_name);
    return SUCCESS;
  }
  else
  {
    LOG_ERROR("Could not create dir %s", dir_name);
    return FAILURE;
  }
} // create_dir()
