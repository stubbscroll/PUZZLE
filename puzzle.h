#ifndef PUZZLE_H
#define PUZZLE_H

#include <SDL/SDL.h>
#include "sdlfont.h"

typedef unsigned char uchar;

#define VERSION_STRING "v1.0"

/*  events */
#define EVENT_NOEVENT   -1
#define EVENT_KEYDOWN   0
#define EVENT_KEYUP     512
#define EVENT_MOUSEDOWN 1024
#define EVENT_MOUSEUP   2048
#define EVENT_MOUSEMOTION 4096
#define EVENT_RESIZE    65536
#define EVENT_QUIT      131072

/*  colours */
#define WHITE32         0xFFFFFF
#define BLACK32         0x000000
#define YELLOW32        0xFFFF00
#define GREEN32         0x00FF00
#define RED32           0xFF6D00
#define DARKBLUE32      0x0000AA
#define BLUE32          0x0000FF
#define GRAY32          0x909090
#define GRAYBLUE32      0xB6B6FF
#define DARKRED32       0xFF0000
#define DARKERRED32     0x920000

/*  global variables (from graphic, mainly) */
extern char gameinfo[65536];

extern int keys[512];
extern int mousebuttons[6];
extern int resx,resy;
extern int thick;
extern int thin;
extern int edgethick;
extern int edgethick2;
extern double aspectratio;
extern SDL_Surface *screen;
extern sdl_font *font;

extern int heyawakethick;
extern int slitherlinkthick;

extern int controlscheme_nurikabe;
extern int controlscheme_akari;
extern int controlscheme_heyawake;
extern int controlscheme_hitori;
extern int controlscheme_picross;
extern int controlscheme_slitherlink;
extern int controlscheme_masyu;
extern int controlscheme_mine;
extern int controlscheme_kuromasu;

extern Uint32 unfilledcol;
extern Uint32 mustprocesscol;
extern Uint32 filledcol;
extern Uint32 filled2col;
extern Uint32 blankcol;
extern Uint32 okcol;
extern Uint32 almostokcol;
extern Uint32 errorcol;
extern Uint32 darkerrorcol;
extern Uint32 darkererrorcol;
extern Uint32 lightcol;
extern Uint32 edgecol;

extern int undokey;
extern int hintkey;
extern int verifykey;

extern int width,height;
extern int startx,starty;

/* score stuff */
extern int numclicks;
extern int usedundo;
extern int usedhint;
extern int normalmove;
extern int timespent;

extern int event_mousebutton;
extern int event_mousex;
extern int event_mousey;
extern int event_mousefromx;
extern int event_mousefromy;

extern int dx[],dy[],dx8[],dy8[];

#define MAXPATH 65536
extern char puzzlepath[MAXPATH];

/*  system stuff */
void error(char *,...);
void logprintf(char *,...);
void resetlog();

double gettime();

int compi(const void *,const void *);

/*  init/shutdown */
void initgr();
void shutdowngr();

/*  non-graphic helper stuff */
void updatescale(int,int,int,int,int);
void getcell(int,int,int *,int *);
void getborder(int,int,int *,int *,int *);
int manhattandist(int,int,int,int);

/* graphics routines */
void clear32(Uint32);
void drawpixel32(int,int,Uint32);
void drawrectangle32(int,int,int,int,Uint32);
void drawhorizontalline32(int,int,int,Uint32);
void drawverticalline32(int,int,int,Uint32);
void drawsolidcell32(int,int,Uint32);
void drawsolidcell32w(int,int,Uint32,int,int,int,int);
void drawcross(int,int,Uint32,Uint32);
void drawnumbercell32(int,int,int,Uint32,Uint32,Uint32);
void drawnumbercell32w(int,int,int,Uint32,Uint32,Uint32,int,int,int,int);
void drawdisc(int,int,double,Uint32,Uint32);
void drawhollowdisc(int,int,double,double,Uint32,Uint32,Uint32);
void sdlrefreshcell(int,int);

int dummyevent();

void messagebox(int,char *,...);

/* stack */

void stackpush(int);
int stackempty();
int stackpop();
int getstackpos();
void setstackpos(int);

/* events */
int getevent();
void anykeypress(void (*)());

/* difficulty */
int getnumericdiff(const char *);

/* timer and score */
void resetscore();
void displayscore(int,int);
void finalizetime();

/* menu system */
void menu();

/* entry point for every game */
void nurikabe(char *,int);
void akari(char *,int);
void heyawake(char *,int);
void hitori(char *,int);
void picross(char *,int);
void slitherlink(char *,int);
void masyu(char *,int);
void hashiwokakero(char *,int);
void yajilin(char *,int);
void mine(char *,int);
void kuromasu(char *,int);

/* launcher */
void launch(char *,int);

#endif  /*  PUZZLE_H */
