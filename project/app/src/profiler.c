/** @file main.c
*
* @brief Function definitions for profiler
*
*/

#include <time.h>
#include <stdint.h>
#include <stdlib.h>

#include "profiler.h"
#include "log.h"

struct timespec start[256];
struct timespec end[256];
uint8_t num_timers = 0;

#define NSEC_PER_SEC (1000000000)

uint8_t profiler_init()
{
  num_timers++;
  reset_timer(num_timers);
  return num_timers;
} // profiler_init()

int8_t start_timer(uint8_t timer)
{
  return clock_gettime(CLOCK_MONOTONIC, &start[timer]);
} // start_timer()

int8_t stop_timer(uint8_t timer)
{
  return clock_gettime(CLOCK_MONOTONIC, &end[timer]);
} // stop_timer()

void reset_timer(uint8_t timer)
{
  start[timer].tv_nsec = end[timer].tv_nsec = 0;
} // reset_timer()

void get_time(uint8_t timer, struct timespec * diff)
{
  diff->tv_nsec = end[timer].tv_nsec - start[timer].tv_nsec;
  diff->tv_sec = end[timer].tv_sec - start[timer].tv_sec;

  if (diff->tv_nsec < 0)
  {
    diff->tv_sec -= 1;
    diff->tv_nsec += NSEC_PER_SEC;
  }
} // get_time()
