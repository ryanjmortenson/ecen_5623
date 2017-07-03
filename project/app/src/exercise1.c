/** @file main.c
*
* @brief Main file for projects
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

#define FIB_LIMIT_FOR_32_BIT 47

// Used the pthread examples for the fib sequence code
#define FIB_TEST(seqCnt, iterCnt)      \
   uint32_t jdx = 0;                   \
   volatile uint32_t idx = 0;          \
   uint32_t fib = 0, fib0 = 0, fib1 = 1; \
   for(idx=0; idx < iterCnt; idx++)    \
   {                                   \
      fib = fib0 + fib1;               \
      while(jdx < seqCnt)              \
      {                                \
         fib0 = fib1;                  \
         fib1 = fib;                   \
         fib = fib0 + fib1;            \
         jdx++;                        \
      }                                \
   }                                   \

int8_t abort_test = 0;
sem_t sem_f10;
sem_t sem_f20;

uint8_t timer = 0;

/*!
* @brief Attempts to execute 10ms of fib sequence used a thread function
* @param[in] param void pointer to parameters
* @return NULL
*/
void * fib_10_func(void * param)
{
  FUNC_ENTRY;

  // Initialize timer diff structer
  struct timespec diff = {.tv_sec = 0, .tv_nsec = 0};

  // Execute loop until abort is set
  while(!abort_test)
  {
    sem_wait(&sem_f10);

    // If semaphore is available and abort set exit without running test
    if (!abort_test)
    {
      FIB_TEST(FIB_LIMIT_FOR_32_BIT, 2200000);
      stop_timer(timer);
      get_time(timer, &diff);
      LOG_HIGH("milliseconds: %f", (float)diff.tv_nsec/1000000);
    }
  }

  LOG_LOW("Exiting f10");
  return NULL;
} // fib_10_func()

/*!
* @brief Attempts to execute 20ms of fib sequence used a thread function
* @param[in] param void pointer to parameters
* @return NULL
*/
void * fib_20_func(void * param)
{
  FUNC_ENTRY;

  // Initialize timer diff structer
  struct timespec diff = {.tv_sec = 0, .tv_nsec = 0};

  // Execute loop until abort is set
  while(!abort_test)
  {
    sem_wait(&sem_f20);

    // If semaphore is available and abort set exit without running test
    if(!abort_test)
    {
      FIB_TEST(FIB_LIMIT_FOR_32_BIT, 4100000);
      stop_timer(timer);
      get_time(timer, &diff);
      LOG_HIGH("milliseconds: %f", (float)diff.tv_nsec/1000000);
    }
  }

  LOG_LOW("Exiting f20");
  return NULL;
} // fib_10_func()

int main()
{
  FUNC_ENTRY;

  /****************************************************************************
   * The RT Clock pthread code was used as an example for the set up of the
   * pthreads with RT priority.
   * *************************************************************************/

  // Setup the timespec structures with the time out requires 10 ms and 20 ms
  struct timespec sleepf10 = {.tv_sec = 0, .tv_nsec = 10000000};
  struct timespec sleepf20 = {.tv_sec = 0, .tv_nsec = 20000000};

  // This structure will be filled out with scheduling parameters
  struct sched_param  sched;
  struct sched_param  f10_sched;
  struct sched_param  f20_sched;
  int32_t f10_policy = 0;
  int32_t f20_policy = 0;
  int32_t res = 0;

  // Pthreads and pthread attributes
  pthread_t fib_10;
  pthread_t fib_20;
  pthread_attr_t sched_attr;

  // Create a profiler
  timer = profiler_init();

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

  // Create semaphores for scheduling
  CHECK_AND_EXIT(res, sem_init(&sem_f10, 0, 0), SUCCESS);
  LOG_LOW("Init sem_f10 successful");
  CHECK_AND_EXIT(res, sem_init(&sem_f20, 0, 0), SUCCESS);
  LOG_LOW("Init sem_f20 successful");

  // Create pthreads fib_10
  sched.sched_priority = rt_max_pri - 1;
  CHECK_AND_EXIT(res, pthread_attr_setschedparam(&sched_attr, &sched), SUCCESS);
  CHECK_AND_EXIT(res, pthread_create(&fib_10, &sched_attr, fib_10_func, NULL), SUCCESS);
  LOG_MED("pthread_create on f10 succeeded");

  // Create pthreads fib_20
  sched.sched_priority = rt_max_pri - 1;
  sched.sched_priority = rt_max_pri - 2;
  CHECK_AND_EXIT(res, pthread_attr_setschedparam(&sched_attr, &sched), SUCCESS);
  CHECK_AND_EXIT(res, pthread_create(&fib_20, &sched_attr, fib_20_func, NULL), SUCCESS);
  LOG_MED("pthread_create on f20 succeeded");

  // Get schedule to display
  CHECK_AND_EXIT(res, pthread_getschedparam(fib_10, &f10_policy, &f10_sched), SUCCESS);
  CHECK_AND_EXIT(res, pthread_getschedparam(fib_20, &f20_policy, &f20_sched), SUCCESS);

  // Display schedule
  LOG_HIGH("FIB10 policy: %d, priority: %d:", f10_policy, f10_sched.sched_priority);
  LOG_HIGH("FIB20 policy: %d, priority: %d:", f20_policy, f20_sched.sched_priority);

  // Start timer
  LOG_HIGH("Starting timer");
  start_timer(timer);

  // Post semaphores
  LOG_LOW("Posting semaphore for both f10 and f20");
  sem_post(&sem_f10);
  sem_post(&sem_f20);

  // Execute schedule (put in for loop so it can be run multiple times)
  for (uint8_t i = 0; i < 1; i++)
  {
    nanosleep(&sleepf20, NULL); sem_post(&sem_f10);
    nanosleep(&sleepf20, NULL); sem_post(&sem_f10);
    nanosleep(&sleepf10, NULL); sem_post(&sem_f20);
    nanosleep(&sleepf10, NULL); sem_post(&sem_f10);
    nanosleep(&sleepf20, NULL); sem_post(&sem_f10);
    nanosleep(&sleepf20, NULL);
  }

  // Set abort flag
  abort_test = 1;

  // Post semaphores so the sem_wait calls stop blocking in each thread
  sem_post(&sem_f20);
  sem_post(&sem_f10);

  // Wait for threads to join
  pthread_join(fib_10, NULL);
  pthread_join(fib_20, NULL);

  // Destroy semaphores
  sem_destroy(&sem_f10);
  sem_destroy(&sem_f20);

  return 0;
} // main()
