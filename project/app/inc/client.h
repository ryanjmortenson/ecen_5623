
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
uint32_t client_init();

#endif /* _CLIENT_H */
