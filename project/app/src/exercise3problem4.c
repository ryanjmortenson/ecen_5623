/** @file exercise3problem4.c
*
* @brief Holds the exercise3problem4 source
*
*/

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "profiler.h"
#include "project_defs.h"

#define MICROSECONDS_PER_SECOND 1000000
#define QUEUE_NAME "/queue"

static uint8_t abort_test = 0;

/*!
* @brief Sends data to message queue
* @param[in] param void pointer to parameters
* @return NULL
*/
void * q_send(void * param)
{
  FUNC_ENTRY;
  uint32_t res = 0;
  char message[] = "This is a message to be sent via queue";

  // Open queue as write only for sending thread
  mqd_t test = mq_open(QUEUE_NAME, O_WRONLY | O_CREAT, S_IRWXU, NULL);
  if (test == -1)
  {
    LOG_ERROR("mq_open failed error: %s", strerror(test));
    exit(1);
  }
  else
  {
    LOG_LOW("Successfully opened queue %s", QUEUE_NAME);
  }
  LOG_FATAL("sizeof message: %u", sizeof(message));
  CHECK_AND_PRINT(res, mq_send(test, message, sizeof(message), 0), SUCCESS);

  // Close message queue
  mq_close(test);

  return NULL;
} // q_send()

/*!
* @brief Reads message queue
* @param[in] param void pointer to parameters
* @return NULL
*/
void * q_recv(void * param)
{
  FUNC_ENTRY;
  uint32_t res = 0;
  char * data;
  struct mq_attr attr;

  // Open queue as read only for sending thread
  mqd_t test = mq_open(QUEUE_NAME, O_RDONLY | O_CREAT, S_IRWXU, NULL);
  if (test == -1)
  {
    LOG_ERROR("mq_open failed error: %s", strerror(errno));
    exit(1);
  }
  else
  {
    LOG_LOW("Successfully opened queue %s", QUEUE_NAME);
  }

  // Get the mq attributes to know how much the max size of the message is
  res = mq_getattr(test, &attr);
  if (res != SUCCESS)
  {
    LOG_ERROR("mq_getattr failed with errno: %s", strerror(errno));
    return NULL;
  }

  // Show size
  LOG_FATAL("mq_msgsize: %d", attr.mq_msgsize);

  // Get a buffer the size of mq_msgsize
  data = malloc(attr.mq_msgsize);
  if (data == NULL)
  {
    // Receive that data from the queue
    LOG_ERROR("malloc failed with error: %s", strerror(errno));
    mq_close(test);
    return NULL;
  }

  // Receive data
  res = mq_receive(test, data, attr.mq_msgsize, NULL);
  if (res == -1)
  {
    LOG_ERROR("mq_receive failed with error: %s", strerror(errno));
    mq_close(test);
    free(data);
    return NULL;
  }

  // Display received data
  LOG_FATAL("Received: %s", data);

  mq_close(test);

  return NULL;
} // q_recv()

int exercise3problem4()
{
  FUNC_ENTRY;

  // Scheduling parameters
  struct sched_param  sched;
  struct sched_param  q_send_sched;
  struct sched_param  q_recv_sched;
  int32_t q_recv_policy = 0;
  int32_t q_send_policy = 0;
  int32_t res = 0;

  // Pthreads and pthread attributes
  pthread_t q_send_thread;
  pthread_t q_recv_thread;
  pthread_attr_t sched_attr;

  // Initialize attitude structure
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

  // Create pthreads q_send_thread
  sched.sched_priority = rt_max_pri - 1;
  CHECK_AND_EXIT(res, pthread_attr_setschedparam(&sched_attr, &sched), SUCCESS);
  CHECK_AND_EXIT(res,
                 pthread_create(&q_send_thread, &sched_attr, q_send, NULL),
                 SUCCESS);
  LOG_MED("pthread_create on q_send_thread succeeded");

  // Create pthreads q_recv_thread
  CHECK_AND_EXIT(res, pthread_attr_setschedparam(&sched_attr, &sched), SUCCESS);
  CHECK_AND_EXIT(res,
                 pthread_create(&q_recv_thread, &sched_attr, q_recv, NULL),
                 SUCCESS);
  LOG_MED("pthread_create on q_recv_thread succeeded");

  // Get schedule to display
  CHECK_AND_PRINT(res,
                 pthread_getschedparam(q_send_thread, &q_send_policy, &q_send_sched),
                 SUCCESS);
  CHECK_AND_PRINT(res,
                 pthread_getschedparam(q_recv_thread, &q_recv_policy, &q_recv_sched),
                 SUCCESS);

  // Display schedule
  LOG_HIGH("q_recv policy: %d, priority: %d:",
           q_recv_policy,
           q_send_sched.sched_priority);
  LOG_HIGH("q_send policy: %d, priority: %d:",
           q_send_policy,
           q_recv_sched.sched_priority);

  // Yield to threads
  usleep(MICROSECONDS_PER_SECOND);
  abort_test = 1;

  // Wait for threads to join
  pthread_join(q_send_thread, NULL);
  pthread_join(q_recv_thread, NULL);

  return 0;
} // main()
