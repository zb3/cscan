#include <stdio.h>

static FILE *metrics = NULL;
  
void metrics_prepare(const char *fname, int connect_timeout)
{
 metrics = fopen(fname, "w");
 fwrite(&connect_timeout, 4, 1, metrics);
}
  
void mev_packet(unsigned long long time, int fd)
{
 fwrite("\xe0", 1, 1, metrics);
 fwrite(&time, 8, 1, metrics);
 fwrite(&fd, 4, 1, metrics);
}
  
void mev_epoll(unsigned long long time, int returned, int timeout)
{
 fwrite("\xe1", 1, 1, metrics);
 fwrite(&time, 8, 1, metrics);
 fwrite(&returned, 4, 1, metrics);
 fwrite(&timeout, 4, 1, metrics);
}
 
void mev_ret(unsigned long long time, unsigned long long start, char kind)
{
 int delta = time-start;
 
 fwrite("\xe2", 1, 1, metrics);
 fwrite(&time, 8, 1, metrics);
 fwrite(&delta, 4, 1, metrics);
 fwrite(&kind, 1, 1, metrics);
}
  
void mev_delta(unsigned long long time, unsigned long long start)
{
 int delta = time-start;
 
 fwrite("\xe3", 1, 1, metrics);
 fwrite(&time, 8, 1, metrics);
 fwrite(&delta, 4, 1, metrics);
}

void mev_sendinfo(unsigned long long time, unsigned long long start, int send_count)
{
 int delta = time-start;
 
 fwrite("\xe4", 1, 1, metrics);
 fwrite(&time, 8, 1, metrics);
 fwrite(&delta, 4, 1, metrics);
 fwrite(&send_count, 4, 1, metrics);
}
  
  
void mev_delinfo(unsigned long long time, unsigned long long start, int del_count)
{
 int delta = time-start;
 
 fwrite("\xe5", 1, 1, metrics);
 fwrite(&time, 8, 1, metrics);
 fwrite(&delta, 4, 1, metrics);
 fwrite(&del_count, 4, 1, metrics);
}
  