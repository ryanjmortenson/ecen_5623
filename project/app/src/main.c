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
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "server.h"
#include "profiler.h"

#define FIB_LIMIT_FOR_32_BIT 47

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

void * fib_10_func(void * param)
{
  FUNC_ENTRY;
  struct timespec diff = {.tv_sec = 0, .tv_nsec = 0};
  uint8_t timer = profiler_init();

  while(!abort_test)
  {
    sem_wait(&sem_f10);
    if (!abort_test)
    {
      start_timer(timer);
      FIB_TEST(FIB_LIMIT_FOR_32_BIT, 2400000);
      stop_timer(timer);
      get_time(timer, &diff);
      LOG_FATAL("milliseconds: %f", (float)diff.tv_nsec/1000000);
    }
  }
  LOG_FATAL("Exiting f10");
  return NULL;
}

void * fib_20_func(void * param)
{
  FUNC_ENTRY;
  struct timespec diff = {.tv_sec = 0, .tv_nsec = 0};
  uint8_t timer = profiler_init();

  while(!abort_test)
  {
    sem_wait(&sem_f20);
    cpu_set_t cpu;
    CPU_SET(0, &cpu);

    if(!abort_test)
    {
      start_timer(timer);
      FIB_TEST(FIB_LIMIT_FOR_32_BIT, 4800000);
      stop_timer(timer);
      get_time(timer, &diff);
      LOG_FATAL("milliseconds: %f", (float)diff.tv_nsec/1000000);
    }
  }
  LOG_FATAL("Exiting f20");
  return NULL;
}

int main()
{
  FUNC_ENTRY;

  // Initialize profiler
  profiler_init();
  struct timespec sleepf10 = {.tv_sec = 0, .tv_nsec = 10000000};
  struct timespec sleepf20 = {.tv_sec = 0, .tv_nsec = 20000000};
  struct sched_param  sched;

  pthread_t fib_10;
  pthread_t fib_20;
  pthread_attr_t sched_attr;

  pthread_attr_init(&sched_attr);
  pthread_attr_setinheritsched(&sched_attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&sched_attr, SCHED_FIFO);

  int32_t rt_max_pri = sched_get_priority_max(SCHED_FIFO);
  uint32_t res = 0;

  sched.sched_priority = rt_max_pri;
  res = sched_setscheduler(getpid(),  SCHED_FIFO, &sched);

  if (res)
  {
    LOG_ERROR("sched_setscheduler failed with errno: %s", strerror(errno));
  }

  res = sem_init(&sem_f10, 0, 0);
  if (res)
  {
    LOG_ERROR("sem_init on f10 failed with errno: %s", strerror(errno));
  }
  else
  {
    LOG_LOW("sem_init on f10 succeeded");
  }

  res = sem_init(&sem_f20, 0, 0);
  if (res)
  {
    LOG_ERROR("sem_init on f20 failed with errno: %s", strerror(errno));
  }
  else
  {
    LOG_LOW("sem_init on f20 succeeded");
  }

  pthread_create(&fib_10, &sched_attr, fib_10_func, NULL);
  pthread_create(&fib_20, &sched_attr, fib_20_func, NULL);
  uint32_t timer = profiler_init();
  struct timespec diff;

  for (uint8_t i = 0; i < 255; i++)
  {
    start_timer(timer);
    FIB_TEST(FIB_LIMIT_FOR_32_BIT, 4800000);
    stop_timer(timer);
    get_time(timer, &diff);
    LOG_FATAL("milliseconds: %f", (float)diff.tv_nsec/1000000);
  }

  for (uint8_t i = 0; i < 1; i++)
  {
    sem_post(&sem_f10); nanosleep(&sleepf10, NULL);
    sem_post(&sem_f10); nanosleep(&sleepf10, NULL);
    sem_post(&sem_f20); nanosleep(&sleepf20, NULL);
    sem_post(&sem_f10); nanosleep(&sleepf10, NULL);
    sem_post(&sem_f10); nanosleep(&sleepf10, NULL);
    nanosleep(&sleepf10, NULL);
  }

  abort_test = 1;

  sem_post(&sem_f20);
  sem_post(&sem_f10);

  pthread_join(fib_10, NULL);
  pthread_join(fib_20, NULL);

  sem_destroy(&sem_f10);
  sem_destroy(&sem_f20);

  return 0;
} // main()
