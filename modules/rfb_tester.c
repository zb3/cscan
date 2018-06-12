#include <stdio.h>
#include <stdlib.h>

#include "socket_utils.h"
#include "rfb.h"

int main(int argc, char **argv)
{
 if (argc < 3)
 {
  printf("%s host port\n", argv[0]);
  return 1;
 }
 
 unsigned int ip = parse_ip4(argv[1]);
 int port = atoi(argv[2]);
 
 int ret = check_vnc(ip, port, 2000, 2000);
 if (ret == 1)
 printf("host ok\n");
 else if (!ret)
 printf("need auth\n");
 else
 printf("error\n");
 
 return 0;
}
