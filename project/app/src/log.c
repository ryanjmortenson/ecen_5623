/** @file log.c
*
* @brief Holds functions used for logging
*
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PATH_SEPERATOR "/"

char * get_basename(char * p_filename)
{
  uint8_t found_last_occurence = 0;

  while (!found_last_occurence)
  {
    char * p_temp = strstr(p_filename, PATH_SEPERATOR);
    if (p_temp != NULL)
    {
      p_filename = p_temp + 1;
    }
    else
    {
      found_last_occurence = 1;
    }
  }
  return p_filename;
} // get_basename()
