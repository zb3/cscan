#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "socket_utils.h"
#include "cba.h"

static char on = 0, has_file = 0;

void print_init(char *enabled, char *cfg)
{
 on = !!strstr(enabled, "print");
 has_file = cba_has_file();
}

void print_result(unsigned int ip, int port)
{
 if (!on) return;
 
 static int num = 0;
 char str[16];
 print_ip4(ip, str);
 
 if (has_file)
 {
  cba_head("");
  cba_str("%s:%d", str, port);
 }
 else
 {
  fprintf(stderr, "got %d result ", ++num); fflush(stderr);
  printf("%s:%d\n", str, port); fflush(stdout);
 }
}