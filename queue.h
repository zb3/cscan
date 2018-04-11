#include <pthread.h>

struct queue
{
 int item_size;
 int capacity;
 int start_idx;
 int next_idx;
 pthread_mutex_t lock;
 pthread_cond_t not_empty;
 pthread_cond_t not_full;
 char *items;
};

void init_queue(struct queue *queue, int capacity, int item_size);
void destroy_queue(struct queue *queue);
void queue_put(struct queue *queue, void *item);
void queue_get(struct queue *queue, void *out);