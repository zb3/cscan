#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>

#include "target.h"
#include "rand.h"
#include "utils.h"

/*
This is the performance bottleneck of cscan...
Currently we use binary search, but it's not fast either

We can't simply choose a random range and then choose an address in that range, because the resulting distribution would be non-uniform since it'd favor addresses from small ranges...
*/
int random_ip(struct range_cfg *ranges)
{
 unsigned int r = gen_rand32(ranges->mask, ranges->max_idx);

 unsigned int start = 0, end = ranges->num;
 unsigned int idx = 0, val = 0;
 
 while(start != end)
 {
  idx = (end+start)>>1;
  val = ranges->ranges[(idx<<1)|1];
  
  if (val == r)
  break;
    
  if (val < r)
  {
   if (idx == end-1)
   {
    //can't be <, but > is ok
    idx++;
    val = ranges->ranges[(idx<<1)|1]; //can be remembered, coz we were here
    break;
   }
   start = idx+1;
  }
  else
  {
   end = idx;
  }
 }
 
 return ranges->ranges[idx<<1] - val + r;
}

int random_port(struct range_cfg *cfg)
{
 unsigned int r = gen_rand32(cfg->mask, cfg->max_idx);
 
 int idx;
 for(idx=0;idx<cfg->num;idx++)
 {
  if (r <= cfg->ranges[(idx<<1)+1])
  return cfg->ranges[idx<<1];
 }
 
 return -1;
}

struct range_cfg *load_range_cfg(const char *file)
{
 FILE *f = fopen(file, "r");
 
 if (!f)
 return NULL;
 
 struct range_cfg *ret = NULL;
 
 int num;
 if (!freadle(&num, 4, f))
 goto out;
 
 //sizeof omits FAM
 ret = malloc(sizeof(struct range_cfg) + num*2*sizeof(unsigned int));
 if (!ret)
 goto out;
 
 ret->num = num;
 
 if (!freadle(&ret->max_idx, 4, f))
 goto out;
 
 if (!freadle(&ret->mask, 4, f))
 goto out;
 
 int r = num << 1;
 unsigned int *ptr = ret->ranges;
 
 while (r --> 0)
 {
  if (!freadle(ptr++, 4, f))
  goto out;
 }
 
 out:
 fclose(f);
 return ret;
}

struct range_cfg *make_port_ranges(const char *portspec)
{
 int num = 0, sum = 0, max = 0, mask = 1;
 int port, points;
 
 unsigned int *data;
 const char *spec  = portspec;
 
 do
 {
  points = 1;
  if (!sscanf(spec,"%d:%d",&port,&points))
  break;
  
  num++;
  max += points;
 }
 while((spec = strstr(spec, ",")) && spec++);
 
 struct range_cfg *ret = malloc(sizeof(struct range_cfg) + num*2*sizeof(unsigned int));
 if (!ret)
 return NULL;
 
 ret->num = num;
 ret->max_idx = max-1;
 
 while(mask<max) mask <<= 1;
 ret->mask = mask-1;
 
 data = ret->ranges;
 spec = portspec;
 
 do
 {
  points = 1;
  if (!sscanf(spec,"%d:%d",&port,&points))
  break;
  
  sum += points;
  *data++ = port;
  *data++ = sum-1;
 }
 while((spec = strstr(spec, ",")) && spec++);

 return ret;
}

