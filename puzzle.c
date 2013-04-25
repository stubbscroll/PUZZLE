/*  puzzle game v0.0 by ruben spaans in 2010-2012
    released under GPL v3.0 */
#include "SDL/SDL.h"
#include <stdio.h>
#include "puzzle.h"

/* music stuff temporarily left here, since it was never committed.
   will be removed in the next commit */
/*
#include <SDL/SDL_mixer.h>
Mix_Chunk *wavfile;
#define AUDIOBUFFER 1024
void music() {
	if(Mix_OpenAudio(44100,AUDIO_S16SYS,2,AUDIOBUFFER)<0) exit(1);
	if(!(wavfile=Mix_LoadWAV("life-test.wav"))) exit(1);
	Mix_PlayChannel(-1,wavfile,0);
}
#undef AUDIOBUFFER
*/

/* if no command line argument, show menu */
int main(int argc,char **argv) {
	int autosolve=0;
	initgr();
 	if(argc>2) autosolve=1;
 	if(!autosolve) resetlog();
	if(argc>1) launch(argv[1],autosolve);
	else {
/*		music();*/
		menu();
	}
	shutdowngr();
	return 0;
}
