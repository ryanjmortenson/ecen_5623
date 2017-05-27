/** @file main.c
*
* @brief Main file for projects
*
*/

#include "log.h"
#include "server.h"

int main()
{
  FUNC_ENTRY;

  // Start the TCP server
  start_serv();

  return 0;
} // main()
