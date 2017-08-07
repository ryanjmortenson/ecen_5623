/** @file utilities.h
*
* @brief Set of helpful utility functions
*
*/

#ifndef _UTILITIES_H
#define _UTILITIES_H

// File permissions used for saving files and creating directories
#define FILE_PERM (S_IRWXU | S_IRWXO | S_IRWXG)

uint32_t get_uname(char * uname_str, uint32_t count);
uint32_t get_timestamp(struct timespec * time, char * timestamp, uint32_t count);

/*!
* @brief Creates a new directory for new captures
* @param dirname pointer to buffer holding first part of directory name
* @param length length of buffer holding directory name
* @return SUCCESS/FAILURE
*/
uint32_t create_dir(char * dir_name);
#endif /* _UTILITIES_H */
