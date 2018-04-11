#pragma once

#include <stdio.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

unsigned long long get_ms();
unsigned long long get_us();
void fail(const char *fmt, ...);
void vinfo(const char *fmt, va_list args);
void info(const char *fmt, ...);

size_t freadle(void * ptr, size_t size, FILE * stream);
size_t fwritele(void * ptr, size_t size, FILE * stream);