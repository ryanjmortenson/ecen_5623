#include <stdint.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "log.h"
#include "project_defs.h"

#define SERVER_PORT (12345)
#define BUFFER_SIZE (256)
#define SOCKET_BACKLOG_LEN (5)

status_t start_serv()
{
  FUNC_ENTRY;

  uint8_t buffer[BUFFER_SIZE];
  int32_t sockfd;
  int32_t newsockfd;
  int32_t bytes_read;
  uint32_t clilen;

  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;

  // Create a socket file descriptor
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0)
  {
     LOG_ERROR("Failed to open socket");
     return FAILURE;
  }

  // Initialize server address
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(SERVER_PORT);

  // Bind the server socket
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
  {
     LOG_ERROR("Could not Bind socket");
     return FAILURE;
  }
  else
  {
    LOG_MED("Bind Successful on port %d", SERVER_PORT);
  }

  // Set the socket as a passive socket
  listen(sockfd, SOCKET_BACKLOG_LEN);
  clilen = sizeof(cli_addr);

  // Wait for incoming connections
  newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

  if (newsockfd < 0)
  {
     LOG_ERROR("Could not accept connection");
     return FAILURE;
  }
  else
  {
    LOG_MED("Accepted Connection from %d on port %d",
            cli_addr.sin_addr.s_addr,
            cli_addr.sin_port);
  }

  // Zero out the buffer and read data into buffer
  bzero(buffer, BUFFER_SIZE);
  bytes_read = read(newsockfd, buffer, BUFFER_SIZE - 1);

  if (bytes_read < 0)
  {
     LOG_ERROR("Could not read from socket");
     return FAILURE;
  }

  LOG_HIGH("Here is the message: %s", buffer);

  return SUCCESS;
} // start_serv()
