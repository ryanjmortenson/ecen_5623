/** @file server.c
*
* @brief Holds functions for a TCP server
*
*/

#include <errno.h>
#include <mqueue.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "project_defs.h"
#include "server.h"

#define SERVER_PORT (12345)
#define BUFFER_SIZE (256)
#define SOCKET_BACKLOG_LEN (5)

// Global abort flag
extern uint32_t abort_test;

// Services can check if a client is connected
uint32_t client_connect = 0;

void * server_service(void * param)
{
  FUNC_ENTRY;

  mqd_t server_queue;
  server_info_t server_msg;
  struct mq_attr attr;
  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;

  int32_t sockfd;
  int32_t newsockfd = 0;
  int32_t res = 0;
  uint32_t clilen;

  // Try to create the queue
  EQ_RET_E(server_queue,
           mq_open(SERVER_QUEUE_NAME, O_RDONLY | O_CREAT, S_IRWXU, NULL),
           -1,
           NULL);

  // Get the server queue attributes
  NOT_EQ_RET_EA(res,
                mq_getattr(server_queue, &attr),
                SUCCESS,
                NULL,
                abort_test)

  // Create a socket file descriptor
  EQ_RET_E(sockfd, socket(AF_INET, SOCK_STREAM, 0), -1, NULL);

  // Initialize server address
  memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(SERVER_PORT);

  // Bind the server socket
  EQ_RET_E(res,
           bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)),
           -1,
           NULL);
  LOG_MED("Bind Successful on port %d", SERVER_PORT);

  // Set the socket as a passive socket
  EQ_RET_E(res, listen(sockfd, SOCKET_BACKLOG_LEN), -1, NULL);
  clilen = sizeof(cli_addr);

  while(!abort_test)
  {
    // Wait for incoming connections
    EQ_RET_E(newsockfd,
             accept(sockfd, (struct sockaddr *)&cli_addr, &clilen),
             -1,
             NULL);

    LOG_MED("Accepted Connection from %d on port %d",
            cli_addr.sin_addr.s_addr,
            cli_addr.sin_port);

    client_connect = 1;

    // Loop until read returns 0 bytes
    while(1)
    {
      EQ_RET_E(res,
               mq_receive(server_queue, (char *)&server_msg, attr.mq_msgsize, NULL),
               -1,
               NULL);
      uint32_t network = htons(server_msg.file_name_len);
      uint32_t buf_size = htons(server_msg.image_buf_len);
      LOG_FATAL("Sending file %s over socket", server_msg.file_name);
      EQ_RET_E(res, write(newsockfd, &network, sizeof(network)), -1, NULL);
      EQ_RET_E(res,
               write(newsockfd, server_msg.file_name, server_msg.file_name_len),
               -1,
               NULL);
      EQ_RET_E(res, write(newsockfd, &buf_size, sizeof(buf_size)), -1, NULL);
      EQ_RET_E(res,
               write(newsockfd, server_msg.image_buf, server_msg.image_buf_len),
               -1,
               NULL);
    }
    client_connect = 0;
  }
  LOG_HIGH("server_service thread exiting");
  mq_close(server_queue);
  close(sockfd);
  if (newsockfd)
  {
    close(newsockfd);
  }
  return NULL;
} // server_service()


uint32_t server_init()
{
  FUNC_ENTRY;
  struct sched_param sched;
  struct sched_param  server_sched;
  pthread_attr_t sched_attr;
  pthread_t server_thread;
  int32_t res = 0;
  int32_t rt_max_pri = 0;
  int32_t server_policy = 0;

  // Initialize the schedule attributes
  PT_NOT_EQ_EXIT(res, pthread_attr_init(&sched_attr), SUCCESS);

  // Set the attributes structure to PTHREAD_EXPLICIT_SCHED and SCHED_FIFO
  PT_NOT_EQ_RET(res,
                pthread_attr_setinheritsched(&sched_attr, PTHREAD_EXPLICIT_SCHED),
                SUCCESS,
                FAILURE);
  PT_NOT_EQ_RET(res,
                pthread_attr_setschedpolicy(&sched_attr, SCHED_FIFO),
                SUCCESS,
                FAILURE);

  // Get the max priority
  EQ_RET_E(rt_max_pri, sched_get_priority_max(SCHED_FIFO), -1, FAILURE);

  // Set the priority to max - 1 for the test thread
  sched.sched_priority = rt_max_pri - 4;
  PT_NOT_EQ_RET(res,
                pthread_attr_setschedparam(&sched_attr, &sched),
                SUCCESS,
                FAILURE);
  // Create pthread
  PT_NOT_EQ_RET(res,
                pthread_create(&server_thread, &sched_attr, server_service, NULL),
                SUCCESS,
                FAILURE);

  // Get the scheduler parameters to display
  PT_NOT_EQ_RET(res,
                pthread_getschedparam(server_thread, &server_policy, &server_sched),
                SUCCESS,
                FAILURE);
  LOG_HIGH("server_service policy: %d, priority: %d",
           server_policy,
           server_sched.sched_priority);
  return SUCCESS;
} // server_init()
