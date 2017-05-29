/** @file log.c
*
* @brief Holds functions used for logging
*
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// See log. for description
char * get_basename(char * p_filename, char * p_path_seperator)
{
  uint8_t found_last_occurence = 0;

  // Loop of string looking for instances of p_path_seperator until
  // the last instance is found.  Use that as basename.
  while (!found_last_occurence)
  {
    char * p_temp = strstr(p_filename, p_path_seperator);
    if (p_temp != NULL)
    {
      p_filename = p_temp + 1;
    }
    else
    {
      found_last_occurence = 1;
    }
  }

  // Return a pointer to the file name
  return p_filename;
} // get_basename()
