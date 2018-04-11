#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>

#include "utils.h"
#include "socket_utils.h"
#include "cba.h"

//.cba file size is limited to 4GB in this implementation

static int fd = -1;
static char *save_dir = ".";
static unsigned long long bytes_written = 0;
static pthread_mutex_t fd_lock = PTHREAD_MUTEX_INITIALIZER;

//main gets tid 0 and writes no head, others go 1,2,3
static __thread char thread_id;
static __thread char *buff;
static __thread int next_code;
static __thread char deferred_buffer[CBA_DEFERBUFF];
static __thread int deferred_bytes;


static int write_hdr_buff(char *buffer, unsigned char type);
static int write_str_buff(char *buffer, int bufsize, char *fmt, va_list args);
static void do_write(void *buffer, int len);


//type & 0x80 = has code 
//chunks can be up to 64K, but we operate on 32K buffer, so we'll send 32K ones
//first byte is L or B depending on endianness, we use native one here

void cba_set_save_dir(char *dir)
{
 save_dir = dir;
}

// currently only files supported, but don't use seek to ensure future stream compatibility
int cba_open_file(char *name)
{
 fd = open(name, O_WRONLY | O_CREAT | O_APPEND, 0777);
 if (fd == -1)
 return 0;
 
 fcntl(fd, F_SETFD, FD_CLOEXEC);
 
 struct stat info;
 if (fstat(fd, &info) == -1)
 return 0;
 
 bytes_written = info.st_size;
 
 if (!bytes_written)
 {
  char bo;
  
  #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  bo = 'B';
  #else
  bo = 'L';
  #endif
  
  write(fd, &bo, 1);
  bytes_written++;
 }
 
 return 1;
}

void cba_close_file()
{
 close(fd);
 fd = -1;
}

int cba_has_file()
{
 return fd != -1;
}

void cba_thread_init(char tid)
{
 thread_id = tid;
 buff = malloc(CBA_MAXBUFF);
 next_code = 0;
}

// is always deferred
void cba_head(char *module)
{
 unsigned char len = strlen(module);
 
 if (fd == -1)
 {
  if (len)
  snprintf(deferred_buffer, CBA_DEFERBUFF, "module: %s\n", module);
  
  deferred_bytes = strlen(deferred_buffer);
 }
 else
 {
  deferred_bytes = write_hdr_buff(deferred_buffer, CBA_HEAD);
  *(unsigned char *)(deferred_buffer+deferred_bytes) = strlen(module);
  deferred_bytes++;
 
  memcpy(deferred_buffer+deferred_bytes, module, len);
  deferred_bytes += len;
 }
}

void cba_code(int c)
{
 next_code = c;
}

void cba_bit(int code)
{
 cba_code(code);
 
 cba_flush();
 
 if (fd == -1)
 {
  printf("bit set for code %d\n", code);  
  return;
 }
 
 char head[64];
 int head_len = write_hdr_buff(head, CBA_BIT);
 
 pthread_mutex_lock(&fd_lock);
 do_write(head, head_len);
 pthread_mutex_unlock(&fd_lock);
}

void cba_defer_str(char *fmt, ...)
{
 va_list args;
 va_start(args, fmt);
 va_end(args);

 if (deferred_bytes)
 {
  if (fd == -1)
  {   
   vsnprintf(deferred_buffer+deferred_bytes, CBA_DEFERBUFF-deferred_bytes, fmt, args);
   deferred_bytes = strlen(deferred_buffer);
   
   strcpy(deferred_buffer+deferred_bytes, "\n");
   deferred_bytes++;
  }
  else
  deferred_bytes += write_str_buff(deferred_buffer+deferred_bytes, CBA_DEFERBUFF-deferred_bytes, fmt, args);
 }
 else
 write_str_buff(NULL, 0, fmt, args); 
}


void cba_flush()
{
 if (deferred_bytes)
 {
  if (fd == -1)
  printf("%s", deferred_buffer);
  else
  do_write(deferred_buffer, deferred_bytes);
 }
 
 deferred_bytes = 0;
}



void cba_str(char *fmt, ...)
{
 va_list args;
 va_start(args, fmt);
 va_end(args);
 
 cba_vstr(fmt, args);
}

void cba_vstr(char *fmt, va_list args)
{
 cba_flush();
 
 write_str_buff(NULL, 0, fmt, args);
}

void cba_data(char *buffer, int len)
{
 cba_flush();
 
 if (fd == -1)
 {
  printf("binary data of length %d\n", len);  
  return;
 }
 
 char head[64];
 int head_len = write_hdr_buff(head, CBA_DATA);
 
 int offset = 0;
 int chunk_len;
 char more = !!len;
 
 pthread_mutex_lock(&fd_lock);
 do_write(head, head_len);
 
 while(1)
 {
  more = len-offset > CBA_MAXBUFF;
  chunk_len = more ? CBA_MAXBUFF : len-offset;
  
  memcpy(buff, buffer+offset, chunk_len);

  do_write(&thread_id, 1);
  do_write(&chunk_len, 2);
  
  if (!chunk_len)
  break;
  
  do_write(buff, chunk_len); 
  offset += chunk_len;
 }
 
 pthread_mutex_unlock(&fd_lock);
} 
//size not int coz we want to reserve possibility for interleaving
void cba_file(char *name, int file)
{
 cba_flush();
 
 if (fd == -1)
 {
  printf("saving file %s\n", name);
  
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", save_dir, name);
  
  int ofd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (ofd == -1)
  info("failed to save file: %s", strerror(errno));
  else
  {
   fcntl(ofd, F_SETFD, FD_CLOEXEC);
   
   int tmp;
   
   while(1)
   {
    tmp = read(file, buff, CBA_MAXBUFF);
    if (tmp <= 0) break;
    
    write_all(ofd, buff, tmp);
   }
   
   close(ofd);
  }
  return;
 }


 char head[64];
 int head_len = write_hdr_buff(head, CBA_FILE);
 
 pthread_mutex_lock(&fd_lock);
 do_write(head, head_len);
 
 unsigned short name_len = strlen(name);
 do_write(&name_len, 2);
 do_write(name, name_len);
  
 unsigned short chunk_len = 0;
 int tmp;
  
 while(1)
 {
  do_write(&thread_id, 1);
  
  tmp = read(file, buff, CBA_MAXBUFF);
  chunk_len = tmp <= 0 ? 0 : tmp;
    
  do_write(&chunk_len, 2);
  
  if (!chunk_len)
  break;
  
  do_write(buff, chunk_len);
 }
 
 pthread_mutex_unlock(&fd_lock);
}


void cba_int32(int num)
{
 cba_flush();
 
 if (fd == -1)
 {
  printf("int32 %d\n", num);  
  return;
 }
 
 char head[64];
 int head_len = write_hdr_buff(head, CBA_INT32);
 
 pthread_mutex_lock(&fd_lock);
 do_write(head, head_len);
 do_write(&num, 4);
 pthread_mutex_unlock(&fd_lock);
} 


void cba_int64(long long num)
{
 cba_flush();
 
 if (fd == -1)
 {
  printf("int64 %lld\n", num);  
  return;
 }
 
 char head[64];
 int head_len = write_hdr_buff(head, CBA_INT64);
 
 pthread_mutex_lock(&fd_lock);
 do_write(head, head_len);
 do_write(&num, 8);
 pthread_mutex_unlock(&fd_lock);
} 


static int write_str_buff(char *buffer, int bufsize, char *fmt, va_list args)
{
 vsnprintf(buff, CBA_MAXBUFF, fmt, args);
 unsigned short length = strlen(buff);
 
 int pos = 0;
 
 if (buffer)
 {
  pos = write_hdr_buff(buffer, CBA_STR);
  length = length >= bufsize-pos-6 ? bufsize-pos-6-1 : length;
  
  buffer[pos+length-1] = 0;
  
  //write len bytes manually
  memcpy(buffer+pos, &length, 2);
  pos += 2;
  
  memcpy(buffer+pos, buff, length);
  pos += length;
  
  //strings can span only one packet
  }
 else if (fd==-1)
 {
  printf("%s\n", buff);
 }
 else
 {
  char head[64];
  pos = write_hdr_buff(head, CBA_STR);
  
  pthread_mutex_lock(&fd_lock);
  do_write(head, pos);
  do_write(&length, 2);
  do_write(buff, length);
  
  //strings can span only one packet
  pthread_mutex_unlock(&fd_lock);
  
 }
 
 return pos;
}



// must be called with lock held
static void do_write(void *buffer, int len)
{
 //todo: stream encrypt buffer having bytes_written
 write_all(fd, buffer, len);
}

static int write_hdr_buff(char *buffer, unsigned char type)
{
 int ret = 0;
 
 *(unsigned char*)buffer = thread_id;
 ret++;
  
 *(unsigned char*)(buffer+1) = type | (next_code ? 0x80 : 0);
 ret++;
 
 if (next_code)
 {
  memcpy(buffer+ret, &next_code, 4);
  ret += 4;
  next_code = 0;
 }

 return ret;
}




