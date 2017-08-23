/** @file client.c
*
* @brief Holds functionality for socket client
*
*/

#include <errno.h>
#include <mqueue.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client.h"
#include "jpeg.h"
#include "log.h"
#include "project_defs.h"
#include "server.h"
#include "utilities.h"

#define SERVER_PORT (12345)
#define ADDRESS "10.0.0.29"

// Inform the main that service is done
static sem_t done;

/*!
* @brief Ensure to receive all bytes on a read
* @param sockfd socket file descriptor
* @param data location to store data
* @param count number of bytes to read
* @return number of bytes read
*/
static inline
uint32_t client_recv(int32_t sockfd, void * data, uint32_t count)
{
  FUNC_ENTRY;

  int32_t bytes = 0;
  int32_t res = 0;

  while (bytes < count)
  {
    EQ_RET_E(res, read(sockfd, data + bytes, count - bytes), -1, -1);
    bytes += res;
  }
  return bytes;
} // client_recv()

/*!
* @brief Shutdown socked
* @param sockfd socket file descriptor
* @return SUCCESS/FAILURE
*/
uint32_t client_dest(int32_t sockfd)
{
  FUNC_ENTRY;

  int32_t res = 0;

  // Shutdown the socket
  LOG_MED("Shutting down socket fd: %d", sockfd);
  EQ_RET_E(res, shutdown(sockfd, SHUT_RDWR), -1, FAILURE);

  return SUCCESS;
} // client_dest()

/*!
* @brief Handles incoming messages on queue
* @param param no data
* @return NULL
*/
void * client_service(void * param)
{
  FUNC_ENTRY;

  struct hostent * server;
  struct sockaddr_in serv_addr;
  int32_t res = 0;
  int32_t sockfd;
  int32_t name_len = 0;
  int32_t buf_len = 0;
  int32_t fd = 0;
  char file_name[FILE_NAME_MAX];
  uint8_t image_buf[IMAGE_NUM_BYTES];

  // Memset the serv_addr struct to 0
  memset((void *)&serv_addr, 0, sizeof(serv_addr));

  // Instantiate socket and get the host name for connection
  EQ_RET_E(sockfd, socket(AF_INET, SOCK_STREAM, 0), -1, NULL);
  EQ_RET_E(server, gethostbyname(ADDRESS), NULL, NULL);

  // Set up the serv_addr structure for connection
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  serv_addr.sin_port = htons(SERVER_PORT);

  // Connect
  LOG_HIGH("Trying connection to %s on port %d", ADDRESS, SERVER_PORT);
  EQ_RET_E(res,
           connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)),
           -1,
           NULL);
  LOG_HIGH("Connection successful");

  // Loop catching the file name size, file name, image buffer size, image buffer
  while(1)
  {
      // Receive the file name
      EQ_RET_E(res, client_recv(sockfd, &name_len, sizeof(name_len)), -1, NULL);
      name_len = htons(name_len);
      if (name_len > FILE_NAME_MAX)
      {
        LOG_ERROR("File name length %d is too long exiting", name_len);
        break;
      }
      EQ_RET_E(res, client_recv(sockfd, file_name, name_len), -1, NULL);
      LOG_LOW("Receiving file name %s", file_name);

      // Receive the buffer
      EQ_RET_E(res, client_recv(sockfd, &buf_len, sizeof(buf_len)), -1, NULL);
      buf_len = htons(buf_len);
      LOG_LOW("Trying to read %d bytes", buf_len);
      EQ_RET_E(res, client_recv(sockfd, image_buf, buf_len), -1, NULL);
      if (buf_len > IMAGE_NUM_BYTES)
      {
        LOG_ERROR("Buffer length %d is too long exiting, buf_len");
        break;
      }
      LOG_HIGH("Received %s", file_name);
      LOG_LOW("Received %d bytes", res);

      // Open file to store contents
      EQ_RET_E(fd, open(file_name, O_CREAT | O_RDWR, FILE_PERM), -1, NULL);
      EQ_RET_E(res, write(fd, image_buf, buf_len), -1, NULL);
      EQ_RET_E(res, close(fd), -1, NULL);
  }
  LOG_HIGH("client_service exiting");
  client_dest(sockfd);
  return NULL;
} // client_service()

uint32_t client_init()
{
  FUNC_ENTRY;
  struct sched_param sched;
  struct sched_param  client_sched;
  pthread_attr_t sched_attr;
  pthread_t client_thread;
  int32_t res = 0;
  int32_t rt_max_pri = 0;
  int32_t client_policy = 0;

  // Semaphore for signaling done
  PT_NOT_EQ_EXIT(res, sem_init(&done, 0, 0), SUCCESS);

  // Try to create directory for storing images
  EQ_RET_E(res, create_dir(DIR_NAME), FAILURE, FAILURE);

  // Initialize the schedule attributes
  PT_NOT_EQ_RET(res, pthread_attr_init(&sched_attr), SUCCESS, FAILURE);

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
                pthread_create(&client_thread, &sched_attr, client_service, NULL),
                SUCCESS,
                FAILURE);

  // Get the scheduler parameters to display
  PT_NOT_EQ_RET(res,
                pthread_getschedparam(client_thread, &client_policy, &client_sched),
                SUCCESS,
                FAILURE);
  LOG_HIGH("client_service policy: %d, priority: %d",
           client_policy,
           client_sched.sched_priority);

  // Wait for thread to join
  pthread_join(client_thread, NULL);
  return SUCCESS;
} // client_init()
