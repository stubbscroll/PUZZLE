/*  puzzle game v0.0 by ruben spaans in 2010-2012
    released under GPL v3.0 */
#include "SDL/SDL.h"
#include <stdio.h>
#include "puzzle.h"

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
