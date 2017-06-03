/** @file main.c
*
* @brief Main file for projects
*
*/

#include <pthread.h>
#include <stdint.h>
#include <time.h>

#include "log.h"
#include "server.h"
#include "profiler.h"

int main()
{
  FUNC_ENTRY;

  profiler_init();
  struct timespec diff;

  pthread_t server_thread;

  start_timer();
  pthread_create(&server_thread,
                 NULL,
                 (void *) start_serv,
                 NULL);
  stop_timer();

  pthread_join(server_thread, NULL);

  LOG_FATAL("Server thread joined");

  get_time(&diff);
  LOG_FATAL("Time s %d, ns %d", diff.tv_sec, diff.tv_nsec);

  return 0;
} // main()
