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

uint32_t create_dir(char * dir_name, uint8_t length)
{
  struct timespec dir_time;
  struct stat st = {0};
  uint32_t res = 0;
  const char * capture = "capture";

  // Check for null pointer
  CHECK_NULL(dir_name);

  // Get the system time to append to directory to be created
  clock_gettime(CLOCK_REALTIME, &dir_time);

  // Get the seconds to create a directory to store captured images
  snprintf(dir_name, length, "%s", capture);

  // Log the newly create directory name
  LOG_LOW("Trying to create directory name: %s", dir_name);

  // Stat to make sure it doesn't exist
  NOT_EQ_RET_E(res, stat(dir_name, &st), -1, FAILURE);

  // Try to create directory now
  NOT_EQ_RET_E(res, mkdir(dir_name, FILE_PERM), SUCCESS, FAILURE);

  return SUCCESS;
} // create_dir()
