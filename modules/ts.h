#pragma once

#include <poll.h>

#ifdef HAVE_TLS
#include "mbedtls/ssl.h"
#endif

typedef struct _tsocket
{
 int fd;
 struct pollfd ps;
 char is_ssl;
 
 #ifdef HAVE_TLS
 mbedtls_ssl_context ctx;
 #endif
} tsocket;

int ts_init();
int ts_socket(tsocket *ts, int fd);
int ts_go_tls(tsocket *ts, unsigned long long deadline); //currently client only
int ts_read_all(tsocket *ts, void *buffer, size_t size, unsigned long long deadline);
int ts_write_all(tsocket *ts, void *buffer, size_t size, unsigned long long deadline);
void ts_close(tsocket *ts); //currently not a graceful shutdown