#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "utils.h"
#include "rand.h"
#include "target.h"

//extern struct range_cfg builtin_ranges;

int main()
{
 static struct range_cfg ranges = {
  3,
  16, 31,
  {
    5, 5,
    45, 11,
    54, 16
  }
 };
 
 static struct range_cfg ports = {
  2,
  11, 15,
  {
    80, 9,
    81, 11
  }
 };

 /*
 //we operate on test version of gen_rand32
 srand(time(NULL));   // should only be called once
 
 int fuzz = 8000000;
 unsigned int atime = 0, btime = 0;
 
 unsigned long long now, now2;
 while (fuzz --> 0)
 {
  int t= rand() % (ranges.max_addr_idx+1);
  gen_rand32(0, t);
  

  now = get_us();
  int r1 = random_ip(&ranges);
  now2 = get_us();
  btime += now2-now;
  
  now = get_us();
  int r2 = random_ip2(&ranges);
  now2 = get_us();
  atime += now2-now;
  
  
  if (r1 != r2)
  {
   printf("%d vs %d for %d\n", r1, r2, t);
   break;
  }
 }
 printf("atime %d, btime %d\n", atime, btime);
 return;
 */

 int t;
 
 for(t=0;t<17;t++)
 {
  gen_rand32(0, t);
  printf("r is %d\n", random_ip(&ranges));
 } 
  
 for(t=0;t<12;t++)
 {
  gen_rand32(0, t);
  printf("p is %d\n", random_port(&ports));
 }
 
 struct range_cfg *parsed = make_port_ranges("443:2,80");
 printf("portspec 2,2,3,443,1,80,2 vs %d,%d,%d,%d,%d,%d,%d\n", parsed->num, parsed->max_idx, parsed->mask, parsed->ranges[0], parsed->ranges[1], parsed->ranges[2], parsed->ranges[3]);
 
 for(t=0;t<3;t++)
 {
  gen_rand32(0, t);
  printf("p is %d\n", random_port(parsed));
 }
 
 struct range_cfg *fromfile = load_range_cfg("test.csw");
 printf("from file 3,16,31,5,5,45,11,54,16 vs %d,%d,%d,%d,%d,%d,%d,%d,%d\n", fromfile->num, fromfile->max_idx, fromfile->mask, fromfile->ranges[0], fromfile->ranges[1], fromfile->ranges[2], fromfile->ranges[3], fromfile->ranges[4], fromfile->ranges[5]);
}

