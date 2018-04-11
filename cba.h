#include <stdarg.h>

#define CBA_HEAD 0
#define CBA_STR 1
#define CBA_FILE 2
#define CBA_DATA 3
#define CBA_INT32 4
#define CBA_INT64 5
#define CBA_BIT 6

#define CBA_MAXBUFF 32768
#define CBA_DEFERBUFF 2048

int cba_open_file(char *name);
int cba_has_file();
void cba_set_save_dir(char *dir);
void cba_close_file();
void cba_thread_init(char tid);
void cba_head(char *module);
void cba_code(int c);
void cba_bit(int code);
void cba_defer_str(char *fmt, ...);
void cba_flush();
void cba_str(char *fmt, ...);
void cba_vstr(char *fmt, va_list args);
void cba_data(char *buffer, int len);
void cba_file(char *name, int file);
void cba_int32(int num);
void cba_int64(long long num);
