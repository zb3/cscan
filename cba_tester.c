#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>

#include "socket_utils.h"
#include "cba.h"

int main(int argc, char **argv)
{
 cba_thread_init(0);
 
 if (argc == 1)
 {
  if (!cba_open_file("test.cba"))
  err(1, "Open fail");
 }
 
 cba_head("mymod");
 cba_defer_str("not visible"); //phails
 
 cba_head("mymod");
 cba_defer_str("visible");
 cba_code(12);
 cba_str("loltest %s:%d", "loly", 55);

 cba_head("mymod2");
 cba_int32(455);
 cba_bit(555);
 
 char *big_data = malloc(66004);
 big_data[0] = big_data[66000] = 123;
 
 cba_data(big_data, 66004);
 
 int fd = open("ephemeral.ctf", O_RDWR | O_CREAT | O_TRUNC);
 unlink("ephemeral.ctf");
 write_all(fd, big_data, 66004);
 lseek(fd, 0, SEEK_SET);
 
 cba_set_save_dir("cba_files");
 
 cba_file("bigdata.com", fd);
 //could we test via memfd?
 //int memfd_create(const char *name, unsigned int flags);
}
