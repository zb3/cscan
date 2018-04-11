#include <stdio.h>
#include <string.h>
#include <err.h>

#include "utils.h"
#include "socket_utils.h"
#include "ts.h"

int main()
{
 ts_init();

 int fd = socket4_connect(parse_ip4("127.0.0.1"), 7546, 3000);
 
 if (fd == -1)
 err(1, "Connect fail");
 
 //switch to ts, but not tls
 
 tsocket ts;
 
 ts_socket(&ts, fd);
 
 unsigned long long deadline = get_ms() + 2000;
 
 char buff[1024];
 if (!ts_read_all(&ts, buff, 10, deadline))
 goto readfail;
 else
 info(buff);
 
 strcpy(buff, "Sendin stuff");
 
 int len = strlen(buff);

 if (!ts_write_all(&ts, &len, 4, deadline))
 goto writefail;
 
 if (!ts_write_all(&ts, buff, len, deadline))
 goto writefail;

 if (!ts_read_all(&ts, buff, 2, deadline))
 goto readfail;
 
 if (!ts_go_tls(&ts, deadline))
 {
  info("Handshake fail");
  goto finish;
 }
 
 strcpy(buff, "Message over TLS!");
 len = strlen(buff);
 
 if (!ts_write_all(&ts, &len, 4, deadline))
 goto writefail;
 
 if (!ts_write_all(&ts, buff, len, deadline))
 goto writefail;
 
 if (!ts_read_all(&ts, &len, 4, deadline))
 goto readfail;
 
 if (!ts_read_all(&ts, buff, len, deadline))
 goto readfail;
 
 buff[len] = 0;
 
 printf("Got: %s\n", buff);
 
 
 
 finish:
 ts_close(&ts);
 return 0;
 
 readfail:
   info("Read fail");
   goto finish;
   
 writefail:
   info("Write fail");
   goto finish;
}