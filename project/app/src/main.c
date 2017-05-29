/** @file main.c
*
* @brief Main file for projects
*
*/
#include <pthread.h>
#include <stdint.h>

#include "log.h"
#include "server.h"

int main()
{
  FUNC_ENTRY;

  pthread_t server_thread;

  pthread_create(&server_thread,
                 NULL,
                 (void *) start_serv,
                 NULL);

  for(uint8_t i = 0; i < 255; i++)
  {
    LOG_ERROR("THREADS!!!!");
  }
  pthread_join(server_thread, NULL);
  LOG_FATAL("Server thread joined");

  return 0;
} // main()
