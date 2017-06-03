/** @file main.c
*
* @brief Function definitions for profiler
*
*/

#include <time.h>
#include <stdint.h>
#include <stdlib.h>

#include "profiler.h"

struct timespec start;
struct timespec end;

void profiler_init()
{
  reset_timer();
} // profiler_init()

int8_t start_timer()
{
  return clock_gettime(CLOCK_MONOTONIC, &start);
} // start_timer()

int8_t stop_timer()
{
  return clock_gettime(CLOCK_MONOTONIC, &end);
} // stop_timer()

void reset_timer()
{
  start.tv_nsec = end.tv_nsec = 0;
} // reset_timer()

void get_time(struct timespec * diff)
{
  diff->tv_nsec = end.tv_nsec - start.tv_nsec;
  diff->tv_sec = end.tv_sec - start.tv_sec;
} // get_time()
