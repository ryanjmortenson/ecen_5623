/** @file client.c
*
* @brief Holds functionality for socket client
*
*/

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "project_defs.h"

#define PORT (12345)
#define ADDRESS "127.0.0.1"

uint32_t client_socket_init(int32_t * sockfd)
{
  FUNC_ENTRY;

  struct hostent * server;
  struct sockaddr_in serv_addr;
  int32_t res = 0;

  // Memset the serv_addr struct to 0
  memset((void *)&serv_addr, 0, sizeof(serv_addr));

  // Instantiate socket and get the host name for connection
  EQ_RET_E(*sockfd, socket(AF_INET, SOCK_STREAM, 0), -1, FAILURE);
  EQ_RET_E(server, gethostbyname(ADDRESS), NULL, FAILURE);

  // Set up the serv_addr structure for connection
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  serv_addr.sin_port = htons(PORT);

  // Connect
  EQ_RET_E(res,
           connect(*sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)),
           -1,
           FAILURE);

  return SUCCESS;
}

uint32_t client_socket_send(int32_t sockfd, void * data, uint32_t count)
{
  FUNC_ENTRY;

  int32_t bytes = 0;

  // Write bytes until finished
  while (bytes < count)
  {
    EQ_RET_E(bytes, write(sockfd, data, count), -1, FAILURE);
  }

  return SUCCESS;
}

uint32_t client_socket_destroy(int32_t sockfd)
{
  FUNC_ENTRY;

  int32_t res = 0;

  // Shutdown the socket
  EQ_RET_E(res, shutdown(sockfd, SHUT_RDWR), -1, FAILURE);

  return SUCCESS;
}
