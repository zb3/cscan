#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include "ts.h"
#include "socket_utils.h"
#include "cba.h"
#include "utils.h"
#include "rfb.h"

#define TLS 18
#define VENCRYPT 19

enum codes
{
 ZERO,
 CONN_FAIL,
 BANNER_FAIL,
 READ_FAIL,
 WRITE_FAIL,
 REASON,
 WIN,
 ONLY_TLS,
 TLS_FAIL,
 HAS_VENCRYPT,
};

static char on = 0;
static int conn_timeout = 2000;
static int read_timeout = 4500;

void rfb_init(char *enabled, char *cfg)
{
 on = !!strstr(enabled, "rfb");
 
 char *opt;
 
 if ((opt=strstr(cfg, "rfb.conn_timeout=")))
 sscanf(opt, "rfb.conn_timeout=%d", &conn_timeout);
 
 if ((opt=strstr(cfg, "rfb.read_timeout=")))
 sscanf(opt, "rfb.read_timeout=%d", &read_timeout);
}

void rfb_help()
{
 printf("rfb.conn_timeout=%d\n"
        "rfb.read_timeout=%d\n", conn_timeout, read_timeout);
}


void rfb_result(unsigned int ip, int port)
{
 if (!on) return;
 
 char ip_str[16];
 print_ip4(ip, ip_str);
 
 char url[64];
 snprintf(url, sizeof(url), "vnc://%s:%d", ip_str, port);
 
 cba_head("rfb");
 cba_defer_str(url);
 
 check_vnc(ip, port, conn_timeout, read_timeout);
}


int check_vnc(unsigned int ip, int port, int conn_timeout, int read_timeout)
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

 char version[16] = {};

 if (!ts_read_all(&ts, version, 12, deadline))
 {
  //ignore this, this is too common
  goto exit;
 }

 int major, minor;

 if (sscanf(version,"RFB %03d.%03d\n",&major,&minor) != 2)
 {
  cba_code(BANNER_FAIL);
  cba_data(version, 12);
  goto exit;
 }
 
 if (major == 3 && (minor == 14 || minor == 16))
 minor -= 10;
 else if ((major == 3 && minor >8))
 minor = 8;
 else if (major>3)
 {
  //proprietary proto,
  major = 3; minor = 8; 
 }

 sprintf(version, "RFB %03d.%03d\n",major,minor);

 if (!ts_write_all(&ts, version, 12, deadline))
 goto write_fail;

 if (major == 3 && minor > 6)
 {
  unsigned char count;
  if (!ts_read_all(&ts, &count, 1, deadline)) goto read_fail;
 
  if (!count)
  goto read_reason;
 
  ret = 0;
 
  int t; unsigned char atype, has_nontls = 0;
  for(t=0;t<count;t++)
  {
   if (!ts_read_all(&ts, &atype, 1, deadline)) goto read_fail;
   
   if (atype==1)
   {
    cba_bit(WIN);
    ret = 1;
    goto exit;
   }
   else if (atype != TLS)
   has_nontls = 1;
   
   if (atype == VENCRYPT)
   cba_bit(HAS_VENCRYPT);
  }
  
  if (!has_nontls)
  {
   cba_bit(ONLY_TLS);
  
   // ok, 1 extra second for TLS
   deadline += 1000;
   
   // we need to choose TLS first
   atype = TLS;
   
   if (!ts_write_all(&ts, &atype, 1, deadline))
   goto write_fail;

   if (!ts_go_tls(&ts, deadline))
   {
    cba_bit(TLS_FAIL);
    goto exit;
   }
   
   if (!ts_read_all(&ts, &count, 1, deadline))
   goto read_fail;
 
   if (!count)
   goto read_reason;
   
   for(t=0;t<count;t++)
   {
    if (!ts_read_all(&ts, &atype, 1, deadline))
    goto read_fail;
   
    if (atype==1)
    {
     cba_bit(WIN);
     ret = 1;
     goto exit;
    }
   }
  }
 }
 else
 {
  int authmode;
  
  if (!ts_read_all(&ts, &authmode, 4, deadline)) goto read_fail;
  
  authmode = ntohl(authmode);
  ret = 0;
  
  if (authmode == 1)
  {
   cba_bit(WIN);
   ret = 1;
   goto exit;
  }
  else if (authmode == 0)
  goto read_reason;
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
 
 read_reason:;
   int len;
   if (!ts_read_all(&ts, &len, 4, deadline)) goto read_fail;
   len = ntohl(len);
   
   char reason_str[512] = {0};
   if (!ts_read_all(&ts, reason_str, MIN(len, 511), deadline)) goto read_fail;
  
   cba_code(REASON);
   cba_str("Error: %s", reason_str);
  
   goto exit;
}  



