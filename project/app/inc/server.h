/** @file server.h
*
* @brief Holds defintions for TCP server
*
*/

#ifndef __SERVER_H__
#define __SERVER_H__

#include "project_defs.h"

// Queue name for images to passed to be sent of TCP socket
# define SERVER_QUEUE_NAME "/server_queue"

// Structure holding information to be passed over TCP socket
typedef struct server_info {
  char file_name[FILE_NAME_MAX];
  uint32_t file_name_len;
  uint8_t * image_buf;
  uint32_t image_buf_len;
} server_info_t;

/*!
* @brief Starts a TCP server
* @return status SUCCESS/FAIL
*/
status_t server_init();

#endif /* __SERVER_H__ */
