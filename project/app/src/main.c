/** @file main.c
*
* @brief Main file for projects
*
*/

#include <stdint.h>
#include <exercise4.h>

int main()
{

  uint32_t res = 0;
#ifdef PROBLEM5
  res = ex4prob5();
#endif

  return res;
}
