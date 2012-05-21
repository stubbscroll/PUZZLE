/*  small dir library by me. warning, it cannot be called again until a
    findclose is issued. */

#include "dir.h"

#define MAX 16384
static char fname[MAX];

dir_fileinfo_t  dir_fileinfo;

#ifdef _WIN32
#include <windows.h>

static HANDLE h;
static WIN32_FIND_DATA f;

/*  return 0 if no file */
int findfirstfile(const char *s) {
  h=FindFirstFile(s,&f);
  
  return 1;
}

int findnextfile() {
  
}
void findclose() {
  FindClose(h);
}
#else
char *findfirstfile(const char *s) {
  puts("TODO");
  return 0;
}
char *findnextfile() {
  puts("TODO");
  return 0;
}
void findclose() {
  puts("TODO");
}
#endif
#undef MAX
