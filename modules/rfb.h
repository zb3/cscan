#pragma once

void rfb_init(char *enabled, char *cfg);
void rfb_help();
void rfb_result(unsigned int ip, int port);
int check_vnc(unsigned int ip, int port, int conn_timeout, int read_timeout);