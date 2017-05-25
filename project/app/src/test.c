#include <stdio.h>
#include <stdint.h>

uint8_t function(uint8_t testVar)
{
  printf("%d\n", testVar);
  return testVar;
}

int main()
{
  printf("test\n");
  function(5);
  return 0;
}
