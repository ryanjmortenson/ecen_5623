
/** @file client.h
*
* @brief Holds functionality for socket client
*
*/

#ifndef _CLIENT_H
#define _CLIENT_H

/*!
* @brief Initialize client socket to talk to server
* @param sockfd pointer to a int32_t to hold socket fd after creation
* @return SUCCESS/FAILURE
*/
uint32_t client_socket_init(int32_t * sockfd, char * add, uint32_t port);

/*!
* @brief Shutdown the client socket
* @param sockfd socket to shutdown
* @return SUCCESS/FAILURE
*/
uint32_t client_socket_destroy(int32_t sockfd);

/*!
* @brief Send data from client socket
* @param sockfd socket to send data over
* @param data data to send over socket
* @param count amount of data to send over socked
* @return SUCCESS/FAILURE
*/
uint32_t client_socket_send(int32_t sockfd, void * data, uint32_t count);

#endif /* _CLIENT_H */
