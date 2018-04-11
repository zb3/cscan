/*
Blocking operations block even if signal was delivered.
*/
#include "queue.h"
#include <stdlib.h>
#include <string.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

void init_queue(struct queue *queue, int capacity, int item_size)
{
 queue->items = malloc(capacity * item_size);
 queue->capacity = capacity;
 queue->item_size = item_size;
 queue->start_idx = -1;
 queue->next_idx = 0;
 
 pthread_mutex_init(&queue->lock, NULL);
 pthread_cond_init(&queue->not_empty, NULL);
 pthread_cond_init(&queue->not_full, NULL); 
}

void destroy_queue(struct queue *queue)
{
 free(queue->items);
 pthread_mutex_destroy(&queue->lock);
 pthread_cond_destroy(&queue->not_empty);
 pthread_cond_destroy(&queue->not_full);
}

void queue_put(struct queue *queue, void *item)
{
 /*
 length... if (==-1) = 0
 length = (capacity + next - start ) % capacity
 full = start == next
 empty = start == -1
 01234567
 
 
 */
 int was_empty = 0;
 
 pthread_mutex_lock(&queue->lock);
 
 //block if full.
 while(queue->start_idx == queue->next_idx)
 pthread_cond_wait(&queue->not_full, &queue->lock);
 
 if (queue->start_idx == -1)
 {
  was_empty = 1;
  queue->start_idx = queue->next_idx;
 }
 
 memcpy(queue->items + queue->item_size*queue->next_idx, item, queue->item_size);
 
 queue->next_idx = (queue->next_idx + 1) % queue->capacity;
 
 pthread_mutex_unlock(&queue->lock);
 
 /*
 Yes, other threads can now invalidate the condition, but if we used signal
 before unlocking, that could happen too - other thread could get the mutex.
 
 So, we check the condition in a loop. But the rule is that condition state
 may not be changed outside the mutex.
 */
 
 if (was_empty)
 pthread_cond_signal(&queue->not_empty);
}

void queue_get(struct queue *queue, void *out)
{
 int was_full = 0;
 
 pthread_mutex_lock(&queue->lock);
 
 while(queue->start_idx == -1)
 pthread_cond_wait(&queue->not_empty, &queue->lock);

 memcpy(out, queue->items + queue->item_size*queue->start_idx, queue->item_size);
 
 if (queue->start_idx == queue->next_idx)
 was_full = 1;
 
 queue->start_idx = (queue->start_idx + 1) % queue->capacity;
 if (queue->start_idx == queue->next_idx)
 {
  queue->start_idx = -1;
  queue->next_idx = 0;
 }
 
 pthread_mutex_unlock(&queue->lock);
 
 if (was_full)
 pthread_cond_signal(&queue->not_full);
}

//I bet I got something wrong here
int not_supported_resize(struct queue *queue, int target_capacity)
{
 pthread_mutex_lock(&queue->lock);
 
 int delta = target_capacity-queue->capacity;
 
 if (delta < 0 && queue->start_idx != -1)
 {      
  //stage1 - trim if needed
  int new_length = MIN(target_capacity, 1+(queue->capacity + queue->next_idx - queue->start_idx - 1)%queue->capacity);
  queue->next_idx = (queue->start_idx + new_length) % queue->capacity;
 
  //stage2 - move elements
  if (queue->next_idx < queue->start_idx)
  {
   //move by delta
   char *start_loc = queue->items + queue->start_idx*queue->item_size;
   char *new_loc = start_loc + delta*queue->item_size;
  
   memmove(start_loc, new_loc, (queue->capacity - queue->start_idx)*queue->item_size);
   queue->start_idx += delta;
  }
  else if (queue->start_idx < queue->next_idx)
  {
   //simply move to the begining
   memmove(queue->items, queue->items + queue->start_idx*queue->item_size, new_length*queue->item_size);
   queue->next_idx -= queue->start_idx;
   queue->start_idx = 0;
  }
 }

 void *new_buff = realloc(queue->items, target_capacity*queue->item_size);
 if (!new_buff)
 {
  pthread_mutex_unlock(&queue->lock);
  return 0;
 }
 
 queue->items = new_buff;
 
 if (delta>0 && queue->start_idx != -1 && queue->next_idx <= queue->start_idx)
 {
  int to_move = MIN(delta, queue->start_idx);
  
  //copy items from the begining to the newly created space
  memcpy(queue->items + queue->capacity*queue->item_size, queue->items, to_move*queue->item_size);
  
  //move remaining items to the begining
  if (queue->next_idx > to_move)
  memmove(queue->items, queue->items + to_move*queue->item_size, (queue->next_idx - to_move)*queue->item_size);
  
  queue->next_idx = (target_capacity + queue->next_idx - to_move) % target_capacity;
 }
 
 queue->capacity = target_capacity;
 
 pthread_mutex_unlock(&queue->lock);
 
 return 1;
}