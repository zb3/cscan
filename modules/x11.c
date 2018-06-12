#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include "ts.h"
#include "socket_utils.h"
#include "cba.h"
#include "utils.h"
#include "x11.h"

#define HELLO "\x6c\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define HELLO_SIZE 12

enum codes
{
 ZERO,
 CONN_FAIL,
 READ_FAIL,
 WRITE_FAIL,
 GOOD,
 KINDA_GOOD
};

static char on = 0;
static int conn_timeout = 2000;
static int read_timeout = 4000;

void x11_init(char *enabled, char *cfg)
{
 on = !!strstr(enabled, "x11");
 
 char *opt;
 
 if ((opt=strstr(cfg, "x11.conn_timeout=")))
 sscanf(opt, "x11.conn_timeout=%d", &conn_timeout);
 
 if ((opt=strstr(cfg, "x11.read_timeout=")))
 sscanf(opt, "x11.read_timeout=%d", &read_timeout);
}

void x11_help()
{
 printf("x11.conn_timeout=%d\n"
        "x11.read_timeout=%d\n", conn_timeout, read_timeout);
}


void x11_result(unsigned int ip, int port)
{
 if (!on) return;
 
 char ip_str[16];
 print_ip4(ip, ip_str);
 
 char url[64];
 snprintf(url, sizeof(url), "x11://%s:%d", ip_str, port);
 
 cba_head("x11");
 cba_defer_str(url);
 
 check_x11(ip, port, conn_timeout, read_timeout);
}


int check_x11(unsigned int ip, int port, int conn_timeout, int read_timeout)
{
 int sock = socket4_connect(ip, port, conn_timeout);
 if (sock == -1)
 {
  cba_bit(CONN_FAIL);
  return -1;
 }

 tsocket ts;
 ts_socket(&ts, sock);
 
 unsigned long long deadline = get_ms() + read_timeout;
 int ret = -1;
 
 if (!ts_write_all(&ts, HELLO, HELLO_SIZE, deadline))
 goto write_fail;
 
 char state;
 if (!ts_read_all(&ts, &state, 1, deadline))
 goto read_fail;
 
 if (state != 1)
 goto exit;
  
 //technically this should work...
 //but we'll do one additional check for proto version

 //xHH now, 5 bytes
 
 char vbuff[5];
 if (ts_read_all(&ts, vbuff, 5, deadline) && vbuff[1] == 11)
 {
  cba_bit(GOOD);
 }
 else
 {
  // first byte was 1 after all
  cba_bit(KINDA_GOOD);
 }


 exit:
 ts_close(&ts);
 return ret;
 
 read_fail:
   cba_bit(READ_FAIL);
   goto exit;
   
 write_fail:
   cba_bit(WRITE_FAIL);
   goto exit;
}  



