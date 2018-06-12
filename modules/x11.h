#pragma once

void x11_init(char *enabled, char *cfg);
void x11_help();
void x11_result(unsigned int ip, int port);
int check_x11(unsigned int ip, int port, int conn_timeout, int read_timeout);