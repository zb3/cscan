#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "socket_utils.h"

static char on = 0;
static int cookie_ports[] = {40033, 41019, 36127, 29438, 0};
static int cookie_timeout = 300;
static int verify_timeout = 1000;

void checkcookie_init(char *enabled, char *cfg)
{
 on = !!strstr(enabled,"cookie");
 
 char *opt;
 
 if ((opt=strstr(cfg, "cookie.timeout=")))
 sscanf(opt, "cookie.timeout=%d", &cookie_timeout);
 
 if ((opt=strstr(cfg, "cookie.ports=")))
 {
  int num = sscanf(opt, "cookie.ports=%d,%d,%d,%d", cookie_ports, cookie_ports+1, cookie_ports+2, cookie_ports+3);
  cookie_ports[num] = 0;
 }
 
 if ((opt=strstr(cfg, "cookie.verify_timeout=")))
 sscanf(opt, "cookie.verify_timeout=%d", &verify_timeout);
}

void checkcookie_help()
{
 printf("cookie.timeout=%d\n"
        "cookie.ports=%d,%d,%d,%d\n"
        "cookie.verify_timeout=%d\n", cookie_timeout, cookie_ports[0], cookie_ports[1], cookie_ports[2], cookie_ports[3], verify_timeout);
}

int check_cookie(unsigned int ip, int port)
{
 if (!on) return 1;
 
 int *cport = cookie_ports;
 int sock;
 
 while(*cport)
 {
  sock = socket4_connect(ip, *cport, cookie_timeout);
  if (sock >= 0)
  {
   close(sock);
   return 0;
  }
  
  cport++;
 }
 
 if (!port)
 return 1;
 
 if (port)
 {
  sock = socket4_connect(ip, port, verify_timeout);
  if (sock >= 0)
  {
   close(sock);
   return 1;
  }
 }
 
 return 0;
}