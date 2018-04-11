#include <stdio.h>
#include <unistd.h>

#include "queue.h"

struct queue q;

void *thread_proc(void *v)
{
 sleep(10);
 int t, tmp;
 for(t=0;t<10;t++)
 {
  queue_get(&q, &tmp);
  printf("ot: got %d\n", tmp);
 }
 
 printf("ot: trying to get...\n");
 queue_get(&q, &tmp);
 printf("ot: got %d\n", tmp);
 
 printf("ot: trying to get...\n");
 queue_get(&q, &tmp);
 printf("ot: got %d\n", tmp);
 
 return NULL;
}

int main()
{
 init_queue(&q, 10, sizeof(int));
 
 pthread_t thread;
 pthread_create(&thread, NULL, thread_proc, NULL);
 
 int tmp;
 
 tmp = 0; queue_put(&q, &tmp);
 tmp = 1; queue_put(&q, &tmp);
 
 queue_get(&q, &tmp);
 printf("got %d\n", tmp);
 
 queue_get(&q, &tmp);
 printf("got %d\n", tmp);
 
 tmp = -1; queue_put(&q, &tmp);
 tmp = 0; queue_put(&q, &tmp);
 
 queue_get(&q, &tmp);
 printf("got %d\n", tmp);
 
 for(tmp=1;tmp<10;tmp++)
 {
  printf("put %d\n", tmp);
  queue_put(&q, &tmp);
 }
 
 printf("trying to put 11\n");
 tmp = 11;
 queue_put(&q, &tmp);
 printf("did it\n");
 
 sleep(10);
 tmp = 12; queue_put(&q, &tmp);

 pthread_join(thread, NULL);
 destroy_queue(&q);
}