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

uint32_t client_socket_init(int32_t * sockfd, char * add, uint32_t port)
{
  FUNC_ENTRY;

  struct hostent * server;
  struct sockaddr_in serv_addr;
  int32_t res = 0;

  // Memset the serv_addr struct to 0
  memset((void *)&serv_addr, 0, sizeof(serv_addr));

  // Instantiate socket and get the host name for connection
  EQ_RET_E(*sockfd, socket(AF_INET, SOCK_STREAM, 0), -1, FAILURE);
  EQ_RET_E(server, gethostbyname(add), NULL, FAILURE);

  // Set up the serv_addr structure for connection
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  serv_addr.sin_port = htons(port);

  // Connect
  LOG_HIGH("Trying connection to %s on port %d", add, port);
  EQ_RET_E(res,
           connect(*sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)),
           -1,
           FAILURE);
  LOG_HIGH("Connection succesful");

  return SUCCESS;
} // client_socket_init()

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
} // client_socket_send()

uint32_t client_socket_dest(int32_t sockfd)
{
  FUNC_ENTRY;

  int32_t res = 0;

  // Shutdown the socket
  LOG_MED("Shutting down socket fd: %d", sockfd);
  EQ_RET_E(res, shutdown(sockfd, SHUT_RDWR), -1, FAILURE);

  return SUCCESS;
} // client_socket_dest()
