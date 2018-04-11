#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>

#include "utils.h"
#include "socket_utils.h"

/*
sometimes we use -2 to signal system errors, when -1 is reserved for failed attempts
*/

unsigned int parse_ip4(const char *ip)
{
 struct in_addr addr = {};
 inet_aton(ip, &addr);
 
 return ntohl(addr.s_addr);
}

void print_ip4(unsigned int ip, char buff[16])
{
 struct in_addr in = {.s_addr = ntohl(ip)};
 inet_ntop(AF_INET, &in, buff, 16); 
}

int set_socket_blocking(int sock, int blocking)
{
 int flags = fcntl(sock, F_GETFL, 0);
 if (flags == -1)
 return -1;
 
 if (blocking)
 flags &= ~O_NONBLOCK;
 else
 flags |= O_NONBLOCK;
 
 if (fcntl(sock, F_SETFL, flags) == -1)
 return -2;
 
 return sock;
}


int socket4_setup(int flags)
{
 int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
 if (sock == -1)
 return -1;

 if (!(flags & NO_LINGER))
 {
  // set linger to 0 to send RST when closing
  // contrary to popular belief, this doesn't work on new linux kernels
 
  struct linger l = {.l_onoff = 1, .l_linger = 0};
  setsockopt(sock, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
 
  // but this undocumented hack, does
 
  int mone = -1;
  setsockopt(sock, IPPROTO_TCP, TCP_LINGER2, &mone, sizeof(mone));
 }
 
 if (!(flags & NO_CLOEXEC))
 fcntl(sock, F_SETFD, FD_CLOEXEC);
 
 return sock;
}


//-2 is socket error, -1 is refused, >= 0 fd ready, <-2 fd in progress

int socket4_try_connect(int sock, unsigned int ip, int port)
{
 struct sockaddr_in addr = {.sin_family = AF_INET};
 addr.sin_addr.s_addr = htonl(ip);
 addr.sin_port = htons(port); 
  
 int ret = -1;
  
 if (set_socket_blocking(sock, 0) < 0)
 goto err;
 
 int cres = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
 if (cres == 0)
 return set_socket_blocking(sock, 1);
 
 if (errno == ECONNREFUSED || errno == ENETUNREACH)
 goto exit;
 else if (errno != EINPROGRESS)
 goto err;
 
 return -sock-2;
 
 exit:
   close(sock);
   return ret;
 
 err:
   ret = -2;
   goto exit;
}

int socket4_connect(unsigned int ip, int port, int timeout)
{
 int sock = socket4_setup(0);
 if (sock < 0)
 return -2;
 
 sock = socket4_try_connect(sock, ip, port);
 
 if (sock >= -2)
 return sock;
 
 sock = -sock-2;
 
 struct pollfd pollfd = {.fd = sock, .events = POLLOUT};
 int pres = poll(&pollfd, 1, timeout);
 
 if (pres > 0 && (pollfd.revents&POLLOUT) && !(pollfd.revents&(POLLERR|POLLHUP)))
 return set_socket_blocking(sock, 1);
 
 close(sock);
 return -1;
}

int read_all(int sock, void *buffer, size_t size, unsigned long long deadline)
{
 size_t bytes_read = 0;
 long long towait;
 int ret;
 
 struct pollfd pollfd = {.fd = sock, .events = POLLIN};
 
 while(bytes_read < size)
 {
  towait = deadline - get_ms();
  if (towait <= 0)
  break;
  
  ret = poll(&pollfd, 1, towait);
  if (ret != 1 || !(pollfd.revents & POLLIN)) break;
  
  ret = read(sock, ((char *)buffer)+bytes_read, size-bytes_read);
  if (ret <= 0) break;
  
  bytes_read += ret;
 }

 return bytes_read<size ? 0 : 1;
}

// no timeout here, but for small messages this shouldn't block anyway
int write_all(int fd, void *buffer, size_t len)
{
 size_t written = 0;
 int ret;
 
 while(written < len)
 {
  ret = write(fd, ((char *)buffer)+written, len-written);
  
  if (ret <= 0)
  return ret;
  
  written += ret;
 }
 
 return len;
}