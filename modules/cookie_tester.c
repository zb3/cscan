#include <stdio.h>

#include "socket_utils.h"
#include "cookie.h"

int main(int argc, char **argv)
{
 if (argc < 2)
 {
  printf("%s host args\n", argv[0]);
  return 1;
 }
 
 unsigned int ip = parse_ip4(argv[1]);
 char *args = argc > 2 ? argv[2] : "";
 
 checkcookie_init("cookie", args);
 
 if (check_cookie(ip, 0))
 {
  printf("Check passed.\n");
  return 0;
 }
 else
 {
  printf("Check failed.\n");
  return 1;
 }
}