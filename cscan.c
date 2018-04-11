#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <err.h>

#include "socket_list.h"
#include "socket_utils.h"
#include "target.h"
#include "queue.h"
#include "utils.h"
#include "modules.h"
#include "log.h"
#include "cba.h"

#ifdef METRICS
#include "metrics.h"
#endif

extern struct range_cfg builtin_ranges;
extern struct range_cfg builtin_ports;

struct result
{
 unsigned int ip;
 int port;
};


int packets_per_second = 520;
int max_pending_results = 50;
int max_pending_sockets = 0;
int connect_timeout = 800;
int socket_list_excess = 1000; //this + max_pending sockets = biggest supported fd. required to allow modules to use files/sockets
int auto_interval = 90;
int connect_interval = 500; //at least X microseconds
unsigned int bind_ip_n = INADDR_ANY;
int bind_port = 32005, bind_max_retries = 5;

int from_stdin = 0;
struct range_cfg *ranges;
struct range_cfg *ports;

int list_only = 0;
int daemonize = 0;

/*
canary can currently only be sent once
otherwise it'd need to use different port
but that could not be reliable...
*/
unsigned int canary_ip = 0;
int canary_port = 80;
int canary_after = 5600;

int stats_every = 1000, stats_every_cba = 10000;
#ifdef METRICS
char *metrics_file = "metrics.dat";
#endif

char *output_cba_file = NULL;
char *output_files_dir = NULL;
char *enabled_modules = ENABLED_MODULES;
char *modules_config = MODULES_CFG;

//
struct queue results_queue;

int done = 0;
int packets_sent = 0;
int result_count = 0;
int event_count = 0;


void queue_result(unsigned int ip, int port)
{
 struct result result = {.ip = ip, .port = port};
 queue_put(&results_queue, &result);
 
 result_count++;
}

void get_result(struct result *out)
{
 queue_get(&results_queue, out);
}


void check_connect_interval()
{
 static unsigned long long last_packet = 0;
 unsigned long long now = get_us();
 
 if (last_packet + connect_interval > now)
 {
  usleep(last_packet+connect_interval-now);
  
  //not so exact, but this is by design
  //this prevents too big intervals if usleep takes longer (I think)
  now = last_packet + connect_interval;
 }
  
 last_packet = now;
}

void print_stats()
{
 static int last_event_count = 0, last_result_count = 0;
 
 log_stats(packets_sent, result_count, event_count-last_event_count, result_count-last_result_count);
 
 last_event_count = event_count;
 last_result_count = result_count;
}

int try_connect(struct socket_list *list)
{
 unsigned int ip = random_ip(ranges);
 int port = random_port(ports);
 
 if (canary_ip && packets_sent == canary_after)
 {
  ip = canary_ip;
  port = canary_port;
  info("sending canary");
 }
 
 int sock = socket4_setup(0);
 if (sock == -1)
 err(1, "Socket fail");
 
 
 // Send SYN packets only once. This is useful for long connection timeouts.
 // EDIT: there's a "timeo", maybe that'd need to be increased first?
 int one = 1;
 setsockopt(sock, IPPROTO_TCP, TCP_SYNCNT, &one, sizeof(one));
 
 // Why? 
 int ttl = 255; 
 setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
 
 // To enable binding to a port and possibly prevent conflicts with normal sockets (?)
 one = 1;
 setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
 
 if (bind_ip_n || bind_port)
 {
  // note IP_FREEBIND wouldn't work, because we can't bind to a particular device without cap_net_raw anyway
  struct sockaddr_in bind_addr = {.sin_family = AF_INET, .sin_addr = {bind_ip_n}};
  
  // can bind fail here @ all? when?
  
  int to_bind_port = bind_port;
  while(to_bind_port < bind_port+bind_max_retries)
  {
   bind_addr.sin_port = htons(to_bind_port);
   
   int ret = bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
   
   if (!ret)
   break;
   
   if (errno == EADDRINUSE)
   log_event("Can't bind to port %d", port);
   else
   err(2, "Bind fail");
   
   if (to_bind_port)
   to_bind_port++;
   else
   break; //if binding to port 0 failed, stop
  }
 }
 
 sock = socket4_try_connect(sock, ip, port);

 packets_sent++;
 if (sock >= -2)
 event_count++;
 
 if (!(packets_sent%stats_every))
 print_stats();
 
 #ifdef METRICS
   unsigned long long now = get_ms();
   
   mev_packet(now, sock < -2 ? -2-sock : sock);
   
   if (sock >= -2)
   mev_ret(now, now, (sock >= 0 ? 1 : 0) << 1 | 1);
 #endif

 if (sock == -2 && (errno == EADDRINUSE || errno == EADDRNOTAVAIL))
 {
  log_event("possible clash");
  return -1;
 }
 else if (sock == -2)
 err(1, "Socket error");
 else if (sock == -1)
 return -1;
 
 if (sock >= 0)
 {
  queue_result(ip, port);
  close(sock);
  return -1;
 }
 
 sock = -sock-2;
 
 if (sock >= list->size)
 fail("WTF! Socket fd too big. System insane...");
 
 add_to_socket_list(list, sock, ip, port, get_ms()+connect_timeout);
 
 return sock;
}

void conn_thread()
{
 struct socket_list sockets;
 init_socket_list(&sockets, max_pending_sockets + socket_list_excess);
 
 struct epoll_event *events;
 events = malloc(max_pending_sockets * sizeof(struct epoll_event));

 int efd = epoll_create1(0);
 if (efd <0 && errno != EINTR)
 err(1, "Can't epoll");
 
 int tfd;
 
 while(!done)
 {
  #ifdef METRICS
  unsigned long long send_start = get_ms();
  int send_count = 0;
  #endif
  
  
  while(sockets.num_sockets < max_pending_sockets)
  {
   if (connect_interval)
   check_connect_interval(); //this can sleep
   
   if ((tfd = try_connect(&sockets)) >= 0)
   {
    struct epoll_event ev;
    ev.events = EPOLLOUT;
    ev.data.fd = tfd;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, tfd, &ev)<0)
    err(1, "Can't add");
    
    #ifdef METRICS
    send_count++;
    #endif
   }
   //info("%d", sockets.num_sockets);
  }
  
  //want to get the first timeout
  
  unsigned long long now = get_ms();
  int timeout = 0;
  
  #ifdef METRICS
  mev_sendinfo(now, send_start, send_count);
  #endif  
  
  if (sockets.entries[sockets.first_fd].timeout > now)
  timeout = sockets.entries[sockets.first_fd].timeout - now;
  
  //we assume that epoll coalesces events
  //but that's also bad for us, what if the c
    
  int nev = epoll_wait(efd, events, max_pending_sockets, timeout);

  #ifdef METRICS
  mev_epoll(get_ms(), nev, timeout);
  #endif
  
  int n;
  for (n = 0; n < nev; ++n)
  {
   int fd = events[n].data.fd;
   int ok = (events[n].events & EPOLLOUT) && !(events[n].events & (EPOLLERR|EPOLLHUP));
   
   if (canary_ip && sockets.entries[fd].ip == canary_ip && sockets.entries[fd].port == canary_port)
   {
    if (!ok)
    log_canary_fail(1);
   }
   else if (ok)
   {
    queue_result(sockets.entries[fd].ip, sockets.entries[fd].port);
   }
   
   event_count++;
   
   #ifdef METRICS
   unsigned long long timenow = get_ms();
   
   mev_ret(timenow, sockets.entries[fd].timeout-connect_timeout, ok ? 2 : 0);
   #endif
   
   close(fd); //removes from epoll tree
   del_from_socket_list(&sockets, fd);
  }

  now = get_ms();
  int fd = sockets.first_fd;
  
  #ifdef METRICS
  unsigned long long del_start = now;
  int del_count = 0;
  #endif
  
  while(fd != -1 && sockets.entries[fd].timeout <= now)
  {
   #ifdef METRICS
   mev_delta(now, sockets.entries[fd].timeout-connect_timeout);
   del_count++;
   #endif
   
   if (sockets.entries[fd].ip == canary_ip && sockets.entries[fd].port == canary_port)
   log_canary_fail(0);
   
   close(fd);
   fd = del_from_socket_list(&sockets, fd);
  }
  
  #ifdef METRICS
  mev_delinfo(get_ms(), del_start, del_count);
  #endif
 }
}

void add_from_stdin()
{
 char ip_str[16];
 int port;
 
 while(scanf("%15[^ :,]%*[ :,]%d ", ip_str, &port) == 2)
 {
  queue_result(parse_ip4(ip_str), port);
 }
}


static int read_int_from_file(char *name)
{
 int fd = open(name, O_RDONLY);
 if (fd != -1)
 {
  char buff[64] = {};
  read(fd, &buff, 63);
  
  int ret = 0;
  if (sscanf(buff, "%d", &ret))
  return ret;

  close(fd);
 }
 
 return -1;
}

static unsigned int get_local_port_range()
{
 int fd = open("/proc/sys/net/ipv4/ip_local_port_range", O_RDONLY);
 if (fd != -1)
 {
  char ranges_buff[64] = {};
  read(fd, ranges_buff, 63);
  
  int start, end;

  if (sscanf(ranges_buff, "%d %d", &start, &end))
  return start << 16 | end;

  close(fd);
 }
 
 return 0;
}

void thread_block_sigint()
{
 //not sure if this is needed
 sigset_t signal_set;
 sigaddset(&signal_set, SIGINT);
 pthread_sigmask(SIG_BLOCK, &signal_set, NULL);
}

void list_thread()
{
 unsigned int ip;
 int port;
 char str[16];

 while(1)
 {
  ip = random_ip(ranges);
  port = random_port(ports);
  
  print_ip4(ip, str);
  printf("%s:%d\n", str, port);
 }
}

void *process_thread(void *tid)
{
 thread_block_sigint();
  
 cba_thread_init((unsigned long int)tid);
 
 struct result result;

 while(1)
 {
  get_result(&result);
  if (!result.ip)
  {
   info("Worker done.");
   break;
  }
  
  modules_exec(result.ip, result.port);
 }
 
 return NULL;
}

void parse_args(int argc, char **argv)
{
 if (argc > 1 && (!strncmp(argv[1], "-h", 2) || !strncmp(argv[1], "--h", 3)))
 {
  printf("-f ranges_file\n"
         "-pf ports_file\n"
         "-p portspec\n\n"
         "-x = don't scan, read results from stdin\n"
         "-l = don't scan, just generate random targets\n"
         "-d = daemonize\n"
         "-o output_cba_file\n"
         "-of output_files_dir = where to save files if .cba output not specified\n"
         "-mod enabled_modules=%s\n"
         "-mcfg modules_config=%s\n\n"
         "-r packets_per_second=%d\n"
         "-t connect_timeout=%d\n"
         "-m max_pending_sockets, overrides packets_per_second\n"
         "-pr max_pending_results=%d\n"
         "-b bind_port=%d\n"
         "-bi bind_ip\n"
         "-bt bind_retries=%d\n"
         "-a auto_interval=%d%%, make sending packets take at least this%% of timeout\n"
         "-ct connect_interval = interval between sending packets in microseconds, overrides auto_interval\n"
         "-e socket_list_excess=%d = this + max_pending sockets = biggest supported fd\n"
         "-c canary_ip\n"
         "-cp canary_port=%d\n"
         "-ca canary_after=%d packets = try to connect to canary_ip and report on failure\n"
         "-st stats_every=%d or %d when using cba output\n", enabled_modules, modules_config, packets_per_second, connect_timeout, max_pending_results, bind_port, bind_max_retries, auto_interval, socket_list_excess, canary_port, canary_after, stats_every, stats_every_cba);
         
  #ifdef METRICS
  printf("-mf metrics_file=%s\n", metrics_file);
  #endif
         
  printf("\n");
  
  
  struct rlimit file_limit;
  if (!getrlimit(RLIMIT_NOFILE, &file_limit))
  printf("max_open_files: %llu\n", (unsigned long long int)file_limit.rlim_max);
  
  int conntrack_max = read_int_from_file("/proc/sys/net/nf_conntrack_max");
  int conntrack_span = read_int_from_file("/proc/sys/net/netfilter/nf_conntrack_tcp_timeout_syn_sent");

  if (conntrack_max == -1)
  {
   conntrack_max = read_int_from_file("/proc/sys/net/ipv4/ip_conntrack_max");
   conntrack_span = read_int_from_file("/proc/sys/net/netfilter/ip_conntrack_tcp_timeout_syn_sent");
  }
  
  if (conntrack_max != -1)
  printf("conntrack_max: %d\n", conntrack_max);
  
  if (conntrack_span != -1)
  printf("conntrack_span: %d seconds\n", conntrack_span);
        
        
  int watches = read_int_from_file("/proc/sys/fs/epoll/max_user_watches");   
  if (watches != -1)
  printf("max_user_watches: %d\n", watches);
  
  
  unsigned int port_range = get_local_port_range();
  if (port_range)
  printf("local_port_range: %d %d\n", port_range >> 16, port_range & 0xffff);

  modules_help();
         
  exit(1);
 }

 int arg_offset = 1;
 
 char *ranges_file = NULL;
 char *ports_file = NULL;
 char *portspec = NULL;
 int new_stats_every = 0;
 
 while(arg_offset < argc)
 {
  if (!strcmp(argv[arg_offset], "-x"))
  from_stdin = 1;
  
  else if (!strcmp(argv[arg_offset], "-d"))
  daemonize = 1;
  
  else if (!strcmp(argv[arg_offset], "-l"))
  list_only = 1;
  
  if (arg_offset >= argc-1)
  break;
  
  if (!strcmp(argv[arg_offset], "-t"))
  connect_timeout = atoi(argv[++arg_offset]);
  
  else if (!strcmp(argv[arg_offset], "-r"))
  packets_per_second = atoi(argv[++arg_offset]);
  
  else if (!strcmp(argv[arg_offset], "-m"))
  {
   packets_per_second = 0;
   max_pending_sockets = atoi(argv[++arg_offset]);
  }
  
  else if (!strcmp(argv[arg_offset], "-pr"))
  max_pending_results = atoi(argv[++arg_offset]);
  
  else if (!strcmp(argv[arg_offset], "-b"))
  bind_port = atoi(argv[++arg_offset]);
  
  else if (!strcmp(argv[arg_offset], "-bi"))
  bind_ip_n = htonl(parse_ip4(argv[++arg_offset]));
  
  else if (!strcmp(argv[arg_offset], "-bt"))
  bind_max_retries = atoi(argv[++arg_offset]);
  
  else if (!strcmp(argv[arg_offset], "-a"))
  auto_interval = atoi(argv[++arg_offset]);
    
  else if (!strcmp(argv[arg_offset], "-ct"))
  {
   auto_interval = 0;
   connect_interval = atoi(argv[++arg_offset]);
  }
  
  else if (!strcmp(argv[arg_offset], "-e"))
  socket_list_excess = atoi(argv[++arg_offset]);
  
  else if (!strcmp(argv[arg_offset], "-c"))
  canary_ip = parse_ip4(argv[++arg_offset]);
  
  else if (!strcmp(argv[arg_offset], "-cp"))
  canary_port = atoi(argv[++arg_offset]);
  
  else if (!strcmp(argv[arg_offset], "-ca"))
  canary_after = atoi(argv[++arg_offset]);
  
  else if (!strcmp(argv[arg_offset], "-mod"))
  enabled_modules = argv[++arg_offset];
  
  else if (!strcmp(argv[arg_offset], "-mcfg"))
  modules_config = argv[++arg_offset];
  
  else if (!strcmp(argv[arg_offset], "-f"))
  ranges_file = argv[++arg_offset];
  
  else if (!strcmp(argv[arg_offset], "-pf"))
  ports_file = argv[++arg_offset];
  
  else if (!strcmp(argv[arg_offset], "-p"))
  portspec = argv[++arg_offset];
  
  else if (!strcmp(argv[arg_offset], "-o"))
  output_cba_file = argv[++arg_offset];
  
  else if (!strcmp(argv[arg_offset], "-o"))
  output_files_dir = argv[++arg_offset];
  
  else if (!strcmp(argv[arg_offset], "-st"))
  new_stats_every = atoi(argv[++arg_offset]);
  
  #ifdef METRICS
  else if (!strcmp(argv[arg_offset], "-mf"))
  metrics_file = argv[++arg_offset];
  #endif
  
  arg_offset++;
 }
 
 if (packets_per_second)
 max_pending_sockets = packets_per_second*connect_timeout/1000;
 else
 packets_per_second = 1000*max_pending_sockets/connect_timeout;
 
 if (auto_interval)
 connect_interval = auto_interval*connect_timeout*10/max_pending_sockets;
 
 if (new_stats_every)
 stats_every = new_stats_every;
 else if (output_cba_file)
 stats_every = stats_every_cba;
  
 if (ranges_file)
 {
  ranges = load_range_cfg(ranges_file);
  if (!ranges)
  fail("Can't read ranges from file.");
 }
 else ranges = &builtin_ranges;
 
 if (ports_file)
 {
  ports = load_range_cfg(ports_file);
  if (!ports)
  fail("Can't read ports from file.");
 }
 else if (portspec)
 {
  ports = make_port_ranges(portspec);
  if (!ports)
  fail("Invalid portspec.");
 }
 else ports = &builtin_ports;
}

void sigint_handler()
{
 //can't lock here
 done = 1;
}

void setup_misc()
{
 //not exhaustive...
 signal(SIGPIPE, SIG_IGN);
 
 struct sigaction sigint_action = {.sa_handler = sigint_handler, .sa_flags = SA_RESTART|SA_RESETHAND};
 sigaction(SIGINT, &sigint_action, NULL);
 
 struct rlimit file_limit;
 if (!getrlimit(RLIMIT_NOFILE, &file_limit))
 {
  file_limit.rlim_cur = file_limit.rlim_max;
  setrlimit(RLIMIT_NOFILE, &file_limit);
 }
 
 int watches = read_int_from_file("/proc/sys/fs/epoll/max_user_watches");
 if (watches > 0 && watches < max_pending_sockets)
 fail("max_user_watches value too low (%d)", watches);  
   
 //should we also prevent bind port conflicts?
 
 if (daemonize)
 {
  pid_t pid;
 
  pid = fork();
 
  if (pid < 0)
  err(1, "Can't fork");
 
  if (pid > 0)
  exit(0);
 
  if (setsid() < 0)
  err(1, "Can't setsid");

  pid = fork();
 
  if (pid < 0)
  err(1, "Can't fork again");
 
  if (pid > 0)
  exit(0);
 
  int fd;
 
  fd = open("/dev/null", O_RDWR);
  if (fd == -1)
  err(1, "Can't open /dev/null");
 
  dup2(fd, STDIN_FILENO);
  dup2(fd, STDOUT_FILENO);
  dup2(fd, STDERR_FILENO);
 
  if (fd > 2) //otherwise we'd close one of the above
  close(fd);
 }
}

char *format_bytes(int bytes)
{
 static char out[64];
 char *units[] = {"B", "KB", "MB", "GB"};
 
 int idx = 0;
 
 while(bytes > 1000 && idx<3)
 {
  bytes /= 1000;
  idx++;
 }
 
 sprintf(out, "%d %s", bytes, units[idx]);
 return out;
}


int main(int argc, char **argv)
{
 parse_args(argc, argv);
 setup_misc();
 
 if (list_only)
 {
  list_thread();
  return 0;
 }
 
 if (output_cba_file && !cba_open_file(output_cba_file))
 err(1, "Can't create .cba file!");
 
 if (!output_cba_file && output_files_dir)
 cba_set_save_dir(output_files_dir);
 
 cba_thread_init(0);
 cba_head("cscan");
 
 #ifdef METRICS
 if (metrics_file)
 metrics_prepare(metrics_file, connect_timeout);
 #endif
 
 modules_init(enabled_modules, modules_config);
 log_stats_every(stats_every);
 
 init_queue(&results_queue, max_pending_results, sizeof(struct result));
 
 pthread_t thread;
 pthread_create(&thread, NULL, process_thread, (void *)1);
 
 if (!from_stdin)
 {
  info("Starting scanner with %d sockets, %d ms connect timeout, ~%d pps, ~%s/s", max_pending_sockets, connect_timeout, packets_per_second, format_bytes(60*packets_per_second));
  conn_thread();
  
  print_stats();
  info("Scanner stopped.");
 }
 else add_from_stdin();
  
 //deliver "poison pills" (to every worker thread, currently one)
 queue_result(0, 0);

 pthread_join(thread, NULL);
 
 cba_close_file();
}




