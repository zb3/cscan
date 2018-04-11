void metrics_prepare(const char *fname, int connect_timeout);
void mev_packet(unsigned long long time, int fd);
void mev_epoll(unsigned long long time, int returned, int timeout);
void mev_ret(unsigned long long time, unsigned long long start, char kind);
void mev_delta(unsigned long long time, unsigned long long start);
void mev_sendinfo(unsigned long long time, unsigned long long start, int send_count);
void mev_delinfo(unsigned long long time, unsigned long long start, int del_count);