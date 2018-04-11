#include <stdlib.h>

#include "socket_list.h"

void init_socket_list(struct socket_list *list, int size)
{
 list->entries = malloc(size * sizeof(struct socket_list_entry));
 list->size = size;
 list->first_fd = list->last_fd = -1;
 list->num_sockets = 0;
}

void destroy_socket_list(struct socket_list *list)
{
 list->size = 0;
 free(list->entries);
}

void add_to_socket_list(struct socket_list *list, int fd, int ip, int port, unsigned long long timeout)
{
 if (fd == list->size)
 return;
 
 list->entries[fd].ip = ip;
 list->entries[fd].port = port;
 list->entries[fd].timeout = timeout;
 list->entries[fd].prev_fd = list->last_fd;
 list->entries[fd].next_fd = -1;
 
 
 if (list->first_fd == -1)
 list->first_fd = list->last_fd = fd;
 else
 {
  list->entries[list->last_fd].next_fd = fd;
  list->last_fd = fd;
 }
 
 list->num_sockets++;
}

int del_from_socket_list(struct socket_list *list, int fd)
{
 if (!list->num_sockets)
 return -1;
 
 if (list->entries[fd].prev_fd != -1)
 list->entries[list->entries[fd].prev_fd].next_fd = list->entries[fd].next_fd;
 else
 list->first_fd = list->entries[fd].next_fd;
 
 if (list->entries[fd].next_fd != -1)
 list->entries[list->entries[fd].next_fd].prev_fd = list->entries[fd].prev_fd;
 else
 list->last_fd = list->entries[fd].prev_fd;
 
 list->num_sockets--;
 
 return list->entries[fd].next_fd;
}
