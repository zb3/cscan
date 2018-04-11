#include <stdio.h>

#include "socket_list.h"

int main()
{
 struct socket_list test;
 init_socket_list(&test, 30);
 
 add_to_socket_list(&test, 0, 0, 0, 0);
 printf("del: %d\n", del_from_socket_list(&test, 0));
 
 add_to_socket_list(&test, 3, 0, 0, 10);
 add_to_socket_list(&test, 1, 0, 0, 20);
 add_to_socket_list(&test, 2, 0, 0, 30);
 add_to_socket_list(&test, 12, 0, 0, 40);
 add_to_socket_list(&test, 8, 0, 0, 50);
 
 printf("del: %d\n", del_from_socket_list(&test, 12));
 
 int fd = test.first_fd;
 printf("first_fd: %d\n", fd);
 
 printf("del: %d\n", fd = del_from_socket_list(&test, fd));
 printf("del: %d\n", fd = del_from_socket_list(&test, fd));
 printf("del: %d\n", fd = del_from_socket_list(&test, fd));
 printf("del: %d\n", fd = del_from_socket_list(&test, fd));
 
 add_to_socket_list(&test, 13, 0, 0, 10);
 add_to_socket_list(&test, 8, 0, 0, 20);
 add_to_socket_list(&test, 3, 0, 0, 30);
 
 del_from_socket_list(&test, 3);
 add_to_socket_list(&test, 0, 0, 0, 30);
 
 fd = test.first_fd;
 printf("first_fd: %d\n", fd);
 
 printf("del: %d\n", fd = del_from_socket_list(&test, fd));
 printf("del: %d\n", fd = del_from_socket_list(&test, fd));
 
 destroy_socket_list(&test);
}