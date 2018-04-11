/*
we can't use select here

needed:
 -MBEDTLS_THREADING_C
 -MBEDTLS_THREADING_PTHREAD
*/

#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>

#include "ts.h"
#include "utils.h"

#ifdef HAVE_TLS

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

#if !defined(MBEDTLS_THREADING_C) || !defined(MBEDTLS_THREADING_PTHREAD)
#error "Need threading"
#endif

#endif


#define CONN_RESET -10
#define ERR_FAILED -11

static char mbed_init = 0;

#ifdef HAVE_TLS
static mbedtls_entropy_context m_entropy;
static mbedtls_ctr_drbg_context m_drbg;
static mbedtls_ssl_config m_client_conf;
#endif

/*
later:
  couldn't we use our rand here to save space?
  need to impl this iface: int mbedtls_ctr_drbg_random( void *p_rng, unsigned char *output, size_t output_len )
*/

int ts_init()
{
 if (mbed_init)
 return 1;
 
 #ifdef HAVE_TLS
 mbedtls_ssl_config_init(&m_client_conf);
 mbedtls_ctr_drbg_init(&m_drbg);
 mbedtls_entropy_init(&m_entropy);
 
 
 if (mbedtls_ctr_drbg_seed(&m_drbg, mbedtls_entropy_func, &m_entropy, (unsigned char *)"cscan", 5))
 return 0;
 
 if (mbedtls_ssl_config_defaults(&m_client_conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT))
 return 0;
 mbedtls_ssl_conf_authmode(&m_client_conf, MBEDTLS_SSL_VERIFY_NONE);
 mbedtls_ssl_conf_rng(&m_client_conf, mbedtls_ctr_drbg_random, &m_drbg);
 
 mbed_init = 1;
 #endif
 
 return 1;
}

#ifdef HAVE_TLS

int ts_bio_recv(void *ts, unsigned char *buf, size_t len)
{
 int ret;
 ret = read(((tsocket *)ts)->fd, buf, len);

 if (ret < 0)
 {
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
  return MBEDTLS_ERR_SSL_WANT_READ;
  
  if (errno == EPIPE || errno == ECONNRESET)
  return CONN_RESET;
  
  return ERR_FAILED;
 }
 
 return ret;
}

int ts_bio_send(void *ts, const unsigned char *buf, size_t len)
{
 int ret;
 ret = write(((tsocket *)ts)->fd, buf, len);
 
 if (ret < 0)
 {
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
  return MBEDTLS_ERR_SSL_WANT_WRITE;
  
  if (errno == EPIPE || errno == ECONNRESET)
  return CONN_RESET;
  
  return ERR_FAILED;
 }

 return ret;
}

#endif


int ts_socket(tsocket *ts, int fd)
{
 ts->fd = fd;
 memset(&ts->ps, 0, sizeof(struct pollfd));
 ts->ps.fd = fd;
 
 // we always operate on nonblocking sockets
 if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0)|O_NONBLOCK) == -1)
 return 0;
 
 ts->is_ssl = 0;
 return 1;
}

static int ts_wait(tsocket *ts, unsigned long long deadline, int want_write)
{
 ts->ps.events = want_write ? POLLOUT : POLLIN;
 
 long long towait = deadline - get_ms();
 if (towait <= 0)
 return 0;
  
 int ret = poll(&ts->ps, 1, towait);
 if (ret != 1 || !(ts->ps.revents & (want_write ? POLLOUT : POLLIN)))
 return 0;
 
 return 1;  
}

// currently only client supported
int ts_go_tls(tsocket *ts, unsigned long long deadline)
{
 #ifdef HAVE_TLS
 
 mbedtls_ssl_init(&ts->ctx);
 
 if (mbedtls_ssl_setup(&ts->ctx, &m_client_conf))
 goto exit;
 
 mbedtls_ssl_set_bio(&ts->ctx, ts, ts_bio_send, ts_bio_recv, NULL);
 
 int ret;
 while ((ret = mbedtls_ssl_handshake(&ts->ctx)))
 {
  if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
  goto exit;
  
  if (!ts_wait(ts, deadline, ret == MBEDTLS_ERR_SSL_WANT_WRITE))
  goto exit;
 }
 
 ts->is_ssl = 1;
 return 1;
 
 exit:
   mbedtls_ssl_free(&ts->ctx);
   return 0;
   
 #else
 return 0;
 #endif
}


int ts_read_all(tsocket *ts, void *buffer, size_t size, unsigned long long deadline)
{
 size_t bytes_read = 0;
 int ret;
 int fd = ts->fd;
 void *ssl = NULL;

 #ifdef HAVE_TLS
 if (ts->is_ssl)
 ssl = &ts->ctx;
 #endif
 
 while (bytes_read < size)
 {
  if (!ssl)
  {
   if (!ts_wait(ts, deadline, 0))
   break;
   
   ret = read(fd, ((char *)buffer)+bytes_read, size-bytes_read);
   if (ret <= 0) break;
   
   bytes_read += ret;
  }
  #ifdef HAVE_TLS
  else
  {
   // can't simply wait for read here. mbed may want to write, or the data may already be there
   
   ret = mbedtls_ssl_read(ssl, ((unsigned char *)buffer)+bytes_read, size-bytes_read);
   
   if (ret == 0 || (ret < 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE))
   break;
   
   if (ret < 0)
   {
    if (!ts_wait(ts, deadline, ret == MBEDTLS_ERR_SSL_WANT_WRITE))
    break;
   }
   else
   bytes_read += ret;
  }
  #endif
 }

 return bytes_read<size ? 0 : 1;
}


int ts_write_all(tsocket *ts, void *buffer, size_t size, unsigned long long deadline)
{
 size_t bytes_written = 0;
 int ret;
 int fd = ts->fd;
 void *ssl = NULL;

 #ifdef HAVE_TLS
 if (ts->is_ssl)
 ssl = &ts->ctx;
 #endif

 while (bytes_written < size)
 {
  if (!ssl)
  {
   if (!ts_wait(ts, deadline, 1))
   break;
   
   ret = write(fd, ((char *)buffer)+bytes_written, size-bytes_written);
   if (ret <= 0) break;
   
   bytes_written += ret;
  }
  #ifdef HAVE_TLS
  else
  {
   // can't simply wait for write here. mbed may want to read
   
   ret = mbedtls_ssl_write(ssl, ((unsigned char *)buffer)+bytes_written, size-bytes_written);
   
   if (ret == 0 || (ret < 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE))
   break;
   
   if (ret < 0)
   {
    if (!ts_wait(ts, deadline, ret == MBEDTLS_ERR_SSL_WANT_WRITE))
    break;
   }
   else
   bytes_written += ret;
  }
  #endif
 }

 return bytes_written<size ? 0 : 1;
}

void ts_close(tsocket *ts)
{
 #ifdef HAVE_TLS
 if (ts->is_ssl)
 {
  // do we need this? not atm, we're done after all.
  /*
   while( ( ret = mbedtls_ssl_close_notify( &ssl ) ) < 0 )
   {
    if( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE )
    err
    
    ts_wait
   }
  */
 }
 #endif
 
 close(ts->fd);
 ts->fd = -1;
 
 #ifdef HAVE_TLS
 if (ts->is_ssl)
 {
  mbedtls_ssl_free(&ts->ctx);
  ts->is_ssl = 0;
 }
 #endif
}