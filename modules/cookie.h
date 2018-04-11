#pragma once

void checkcookie_init(char *enabled, char *cfg);
void checkcookie_help();
int check_cookie(unsigned int ip, int port);
