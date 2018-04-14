#include <stdio.h>
#include <string.h>

#include "modules.h"
#include "ts.h"

/*
Handling options should not be done here for clarity, only interaction between modules should be visible from here.
*/


#ifdef HAVE_print
#include "print.h"
#endif

#ifdef HAVE_checkcookie
#include "cookie.h"
#endif


void modules_init(char *enabled, char *cfg)
{
 ts_init();

 #ifdef HAVE_print
 print_init(enabled, cfg);
 #endif
 
 #ifdef HAVE_checkcookie
 checkcookie_init(enabled, cfg);
 #endif
}

void modules_help()
{
 if (strcmp(MODULES_STR, ""))
 printf("\nCompiled modules: %s\n", MODULES_STR);
 
 #if defined(HAVE_checkcookie)
  printf("\nModule options:\n");
  
  #ifdef HAVE_checkcookie
  checkcookie_help();
  #endif

 #endif
}

void modules_exec(unsigned int ip, int port)
{
 #ifdef HAVE_checkcookie
 if (check_cookie(ip, port))
 {
 #endif

 #ifdef HAVE_print
 print_result(ip, port);
 #endif
 
 #ifdef HAVE_checkcookie
 }
 else
 {
  //sth
 }
 #endif

}