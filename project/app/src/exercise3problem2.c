/** @file exercise3problem2.c
*
* @brief Holds the exercise3problem2 source
*
*/

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "profiler.h"
#include "project_defs.h"

#define RAND_SLEEP 1000
#define MICROSECONDS_PER_SECOND 1000000
#define DISPLAY_STRING "pitch: %u, roll: %u, yaw: %u, timestamp (s): %u, timestamp (ns): %u"

static uint8_t abort_test = 0;

// Structure used in threads
struct {
  uint32_t pitch;
  uint32_t roll;
  uint32_t yaw;
  struct timespec timestamp;
} attitude;

// Mutex for accessing
pthread_mutex_t attitude_mutex = PTHREAD_MUTEX_INITIALIZER;

/*!
* @brief Updates the attitude structure
* @param[in] param void pointer to parameters
* @return NULL
*/
void * t_update(void * param)
{
  FUNC_ENTRY;
  uint32_t res = 0;
  CHECK_AND_PRINT(res, pthread_mutex_lock(&attitude_mutex), SUCCESS);

  // Loop until test is aborted
  while(!abort_test)
  {
    // Get random info and timestamp if mutex locked
    if (!res)
    {
      attitude.pitch = rand();
      attitude.roll = rand();
      attitude.yaw = rand();
      clock_gettime(CLOCK_MONOTONIC, &attitude.timestamp);

      // Display new random info
      LOG_LOW(DISPLAY_STRING,
              attitude.pitch,
              attitude.roll,
              attitude.yaw,
              attitude.timestamp.tv_sec,
              attitude.timestamp.tv_nsec);

      // Try to unlock mutex
      CHECK_AND_PRINT(res, pthread_mutex_unlock(&attitude_mutex), SUCCESS);

      // Sleep for a random amount between 0 to RAND_SLEEP
      usleep(rand() % RAND_SLEEP);
    }
    else
    {
      LOG_FATAL("Mutex lock failed in t_update");
    }
  }

  return NULL;
} // t_update()

/*!
* @brief Reads attitude structure
* @param[in] param void pointer to parameters
* @return NULL
*/
void * t_read(void * param)
{
  FUNC_ENTRY;
  uint32_t res = 0;
  CHECK_AND_PRINT(res, pthread_mutex_lock(&attitude_mutex), SUCCESS);

  // Loop until test is aborted
  while(!abort_test)
  {
    // Get random info and timestamp if mutex locked
    if (!res)
    {

      // Display info from shared data structure
      LOG_LOW(DISPLAY_STRING,
              attitude.pitch,
              attitude.roll,
              attitude.yaw,
              attitude.timestamp.tv_sec,
              attitude.timestamp.tv_nsec);

      // Try to unlock mutex
      CHECK_AND_PRINT(res, pthread_mutex_unlock(&attitude_mutex), SUCCESS);

      // Sleep for a random amount between 0 to RAND_SLEEP
      usleep(rand() % RAND_SLEEP);
    }
    else
    {
      LOG_FATAL("Mutex lock failed t_read");
    }
  }

  return NULL;
} // t_read()

int exercise3problem2()
{
  FUNC_ENTRY;

  // Scheduling parameters
  struct sched_param  sched;
  struct sched_param  t_update_sched;
  struct sched_param  t_read_sched;
  int32_t t_read_policy = 0;
  int32_t t_update_policy = 0;
  int32_t res = 0;

  // Pthreads and pthread attributes
  pthread_t t_update_thread;
  pthread_t t_read_thread;
  pthread_attr_t sched_attr;

  // Initialize attitude structure
  attitude.pitch = 1;
  attitude.roll = 2;
  attitude.yaw = 3;
  clock_gettime(CLOCK_MONOTONIC, &attitude.timestamp);

  // Initialize the pthread attr structure with an explicit schedule of FIFO
  CHECK_AND_EXIT(res, pthread_attr_init(&sched_attr), SUCCESS);
  CHECK_AND_EXIT(res,
                 pthread_attr_setinheritsched(&sched_attr, PTHREAD_EXPLICIT_SCHED),
                 SUCCESS);
  CHECK_AND_EXIT(res,
                 pthread_attr_setschedpolicy(&sched_attr, SCHED_FIFO),
                 SUCCESS);

  // Get the max priority to assign to threads and main process
  int32_t rt_max_pri = sched_get_priority_max(SCHED_FIFO);

  // Set the scheduler for the main thread
  sched.sched_priority = rt_max_pri;
  CHECK_AND_EXIT(res, sched_setscheduler(getpid(), SCHED_FIFO, &sched), SUCCESS);
  CHECK_AND_EXIT(res, pthread_attr_setschedparam(&sched_attr, &sched), SUCCESS);

  // Create pthreads t_update_thread
  sched.sched_priority = rt_max_pri - 1;
  CHECK_AND_EXIT(res, pthread_attr_setschedparam(&sched_attr, &sched), SUCCESS);
  CHECK_AND_EXIT(res,
                 pthread_create(&t_update_thread, &sched_attr, t_update, NULL),
                 SUCCESS);
  LOG_MED("pthread_create on t_update_thread succeeded");

  // Create pthreads t_read_thread
  CHECK_AND_EXIT(res, pthread_attr_setschedparam(&sched_attr, &sched), SUCCESS);
  CHECK_AND_EXIT(res,
                 pthread_create(&t_read_thread, &sched_attr, t_read, NULL),
                 SUCCESS);
  LOG_MED("pthread_create on t_read_thread succeeded");

  // Get schedule to display
  CHECK_AND_EXIT(res,
                 pthread_getschedparam(t_update_thread, &t_read_policy, &t_update_sched),
                 SUCCESS);
  CHECK_AND_EXIT(res,
                 pthread_getschedparam(t_read_thread, &t_update_policy, &t_read_sched),
                 SUCCESS);

  // Display schedule
  LOG_HIGH("t_read policy: %d, priority: %d:",
           t_read_policy,
           t_update_sched.sched_priority);
  LOG_HIGH("t_update policy: %d, priority: %d:",
           t_update_policy,
           t_read_sched.sched_priority);

  // Yield to threads
  usleep(MICROSECONDS_PER_SECOND);
  abort_test = 1;

  // Wait for threads to join
  pthread_join(t_update_thread, NULL);
  pthread_join(t_read_thread, NULL);

  return 0;
} // main()
