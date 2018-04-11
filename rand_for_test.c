#include <unistd.h>
#include <fcntl.h>

int gen_rand32(unsigned int mask, unsigned int max)
{
 static int val = 0;
 
 if (mask==0)
 val = max;
 
 return val;
}