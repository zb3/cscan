#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

unsigned long long get_ms()
{
 struct timespec spec;
 clock_gettime(CLOCK_MONOTONIC, &spec);

 return spec.tv_sec*1000 + spec.tv_nsec/1000000;
}

unsigned long long get_us()
{
 struct timespec spec;
 clock_gettime(CLOCK_MONOTONIC, &spec);

 return spec.tv_sec*1000000 + spec.tv_nsec/1000;
}

void fail(const char *fmt, ...)
{
 va_list args;
 va_start(args, fmt);
 vfprintf(stderr, fmt, args);
 va_end(args);
 
 fprintf(stderr, "\n");
 fflush(stderr);
 
 exit(1);
}

void vinfo(const char *fmt, va_list args)
{
 vfprintf(stderr, fmt, args);
 fprintf(stderr, "\n");
 fflush(stderr);
}

void info(const char *fmt, ...)
{
 va_list args;
 va_start(args, fmt);
 va_end(args);
 
 vinfo(fmt, args);
}

size_t freadle(void * ptr, size_t size, FILE * stream)
{
 size_t ret = fread(ptr, size, 1, stream);
 
 #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
 if (ret)
 {
  if (size == 2) *(uint16_t *)ptr = __builtin_bswap16(*((uint16_t *)ptr));
  else if (size == 4) *(uint32_t *)ptr = __builtin_bswap32(*((uint32_t *)ptr));
  else if (size == 8) *(uint64_t *)ptr = __builtin_bswap64(*((uint64_t *)ptr));
 }
 #endif
 
 return ret;
}

size_t fwritele(void * ptr, size_t size, FILE * stream)
{
 size_t ret = 0;
 
 #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
 char *pptr = ptr;
 pptr += size;
 
 while(size --> 0)
 {
  ret += fwrite(--pptr, 1, 1, stream);
 }
 
 ret /= size;
 
 #else
 ret = fwrite(ptr, size, 1, stream);
 #endif
 
 
 return ret;
}