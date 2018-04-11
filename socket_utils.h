#pragma once

#include <stdlib.h>

#define NO_LINGER 1
#define NO_CLOEXEC 2

unsigned int parse_ip4(const char *ip);
void print_ip4(unsigned int ip, char *buff);
int set_socket_blocking(int fd, int blocking);
int socket4_setup(int flags);
int socket4_try_connect(int sock, unsigned int ip, int port);
int socket4_connect(unsigned int ip, int port, int timeout);
int read_all(int sock, void *buffer, size_t size, unsigned long long deadline);
int write_all(int fd, void *buff, size_t len);