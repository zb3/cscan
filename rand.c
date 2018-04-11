#include <unistd.h>
#include <fcntl.h>

int gen_rand32(unsigned int mask, unsigned int max)
{
 static int fd = 0;
 
 if (!fd)
 fd = open("/dev/urandom", O_RDONLY);
 
 if (fd == -1)
 return -1;
 
 unsigned int ret;
 
 do
 {
  read(fd, &ret, 4);
  ret &= mask;
 } while (ret > max);

 return ret;
}