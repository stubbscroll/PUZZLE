/*  puzzle game v0.0 by ruben spaans in 2010-2012
    released under GPL v3.0 */
#include "SDL/SDL.h"
#include <stdio.h>
#include "puzzle.h"

char *getid(char *s) {
  int i;
  for(i=strlen(s)-1;i>=0 && s[i]!='/' && s[i]!='\\';--i);
  return s+i+1;
}

void launch(char *s,int autosolve) {
  char *t=getid(s);
  if     (t[0]=='n' && t[1]=='u' && t[2]=='r') nurikabe(s,autosolve);
  else if(t[0]=='a' && t[1]=='k' && t[2]=='a') akari(s,autosolve);
  else if(t[0]=='h' && t[1]=='e' && t[2]=='y') heyawake(s,autosolve);
  else if(t[0]=='h' && t[1]=='i' && t[2]=='t') hitori(s,autosolve);
  else if(t[0]=='p' && t[1]=='i' && t[2]=='c') picross(s,autosolve);
  else if(t[0]=='s' && t[1]=='l' && t[2]=='i') slitherlink(s,autosolve);
  else if(t[0]=='m' && t[1]=='a' && t[2]=='s') masyu(s,autosolve);
  else if(t[0]=='h' && t[1]=='a' && t[2]=='s') hashiwokakero(s,autosolve);
  else if(t[0]=='y' && t[1]=='a' && t[2]=='j') yajilin(s,autosolve);
  else if(t[0]=='m' && t[1]=='i' && t[2]=='n') mine(s,autosolve);
  else if(t[0]=='k' && t[1]=='u' && t[2]=='r') kuromasu(s,autosolve);
}

/* if no command line argument, show menu */
int main(int argc,char **argv) {
  int autosolve=0;
	initgr();
  if(argc>2) autosolve=1;
  if(!autosolve) resetlog();
	if(argc>1) launch(argv[1],autosolve);
	else menu();
  shutdowngr();
  return 0;
}
