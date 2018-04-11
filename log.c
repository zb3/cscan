#include <time.h>

#include "cba.h"
#include "utils.h"

/*
Name is a bit misleading - this uses cba to log internal cscan events.

Original idea was to make info() print to cba, but to make tid=0 print to stderr if no file
maybe that was better?
*/

#define STATS_EVERY 10
#define STAT_TIME 11

#define EVENT 20
#define CANARY_FAIL 21
#define CANARY_FAIL_RESPONSE 22

void log_stats_every(int stats_every)
{
 if (cba_has_file())
 {
  cba_code(STATS_EVERY);
  cba_int32(stats_every);
 }
}

void log_stats(int packets, int results, int new_events, int new_results)
{
 info("%d packets sent, %d results received, %d new events", packets, results, new_events);

 if (cba_has_file())
 {
  cba_code(STAT_TIME); cba_int64(time(NULL));
  cba_int32(new_events); cba_int32(new_results);
 }
}

void log_canary_fail(int got_reply)
{
 info("canary FAIL: %s", got_reply ? "invalid response" : "timed out");

 if (cba_has_file())
 {
  cba_bit(got_reply ? CANARY_FAIL_RESPONSE : CANARY_FAIL);
 }
}

void log_event(char *fmt, ...)
{
 va_list args, args2;
 va_start(args, fmt);
 va_end(args);
 va_copy(args2, args);
 va_end(args2);
 
 vinfo(fmt, args);
 
 if (cba_has_file())
 {
  cba_code(EVENT);
  cba_vstr(fmt, args2);
 } 
}
