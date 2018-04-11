/*
A data structure to allow O(1) lookups and O(1) iteration (kinda).

cons:
-limits max fd value
-memory expensive

of course we assume that timeouts are increasing
*/

struct socket_list
{
 struct socket_list_entry *entries;
 int size;
 int first_fd;
 int last_fd;
 int num_sockets;
};

struct socket_list_entry
{
 int prev_fd;
 int next_fd;
 unsigned long long timeout;
 unsigned int ip;
 int port;
};

void init_socket_list(struct socket_list *list, int size);
void destroy_socket_list(struct socket_list *list);
void add_to_socket_list(struct socket_list *list, int fd, int ip, int port, unsigned long long timeout);
int del_from_socket_list(struct socket_list *list, int fd);