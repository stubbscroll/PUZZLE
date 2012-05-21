#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
  #include <windows.h>
#endif
/*  generate batch file for automatic testing of solver */

/*  max string length -1 */
#define MAXSTR 32
/*  max number of files */
#define MAXFILES 250000

char files[MAXFILES][MAXSTR];
int n;

#ifdef _WIN32
int main() {
  int i;
  n=0;
  WIN32_FIND_DATA f;
  HANDLE hfind;
  hfind=FindFirstFile("*.txt",&f);
  if(INVALID_HANDLE_VALUE==hfind) return 0;
  do {
    if(!strcmp(f.cFileName,".") || !strcmp(f.cFileName,"..")) continue;
    if(strlen(f.cFileName)<MAXSTR) strcpy(files[n++],f.cFileName);
  } while(FindNextFile(hfind,&f));
  qsort(files,n,MAXSTR,(int(*)(const void *,const void *))strcmp);
  for(i=0;i<n;i++) printf("@puzzle puzzles\\%s x\n",files[i]);
  return 0;
}
#else
int main() {
  printf("unfortunately, i don't know (yet) how to dir in *n?x\n)";
  return 0;
}
#endif
