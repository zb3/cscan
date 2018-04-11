void log_stats_every(int stats_every);
void log_stats(int packets, int results, int new_events, int new_results);
void log_bind_fail(int port);
void log_canary_fail(int got_response);
void log_event(char *fmt, ...);