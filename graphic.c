#include "SDL/SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include "puzzle.h"
#include "sdlfont.h"

#define GRAPHIC_BPP 32

char gameinfo[65536];                 /* name+credits for current game */

int resx=800,resy=600;                /*  resolution (window size) */
double aspectratio=1.0;               /*  >1=wider, 1=square <1=taller */
int thick=1;                          /*  thickness of grid line */
int thin=1;                           /*  thickness of thinner grid line */
int edgethick=5;                      /*  thickness of edge between cells */
int edgethick2=3;                     /*  thickness of parallel edges between cells */
static int fullscreen=0;              /*  1=fullscreen 0=windowed */

/*  special overriding options for specific games */
int heyawakethick=-1;
int slitherlinkthick=-1;

Uint32 unfilledcol=GRAY32;
Uint32 mustprocesscol=GRAYBLUE32;
Uint32 blankcol=WHITE32;
Uint32 filledcol=BLACK32;
Uint32 filled2col=BLUE32;
Uint32 okcol=GREEN32;
Uint32 almostokcol=YELLOW32;
Uint32 errorcol=RED32;
Uint32 darkerrorcol=DARKRED32;
Uint32 darkererrorcol=DARKERRED32;
Uint32 lightcol=GREEN32;
Uint32 edgecol=GRAY32;

/*  control schemes for the different games */
int controlscheme_nurikabe=0;
int controlscheme_akari=0;
int controlscheme_heyawake=0;
int controlscheme_hitori=0;
int controlscheme_picross=0;
int controlscheme_slitherlink=0;
int controlscheme_mine=0;
int controlscheme_kuromasu=0;

/*  keyboard settings */

int undokey=SDLK_BACKSPACE;
int hintkey=SDLK_h;
int verifykey=SDLK_v;

static int videoflags;

SDL_Surface *screen;
sdl_font *font;

int width,height;                     /*  size of cell (including line) */
int startx,starty;                    /*  top left of grid area */

static int bpp;                       /*  _bytes_ per pixel */
static int pitch;                     /*  bytes per scanline */
static Uint32 *pixels;                /*  shortcut pointer to pixels */

int keys[512];
int mousebuttons[6];

/*  length of a constant string */
#define GRAPHIC_FS 1024
static char s[GRAPHIC_FS];
static char t[GRAPHIC_FS];
static char u[GRAPHIC_FS];

/* let's supply dx/dy arrays */
/* dx is right, down, left, up, center */
int dx[]={1,0,-1,0,0},dy[]={0,1,0,-1,0};
int dx8[]={1,0,-1,0,1,1,-1,-1},dy8[]={0,1,0,-1,1,-1,1,-1};

/* path for puzzles */
char puzzlepath[MAXPATH];

void error(char *s,...) {
  static char t[GRAPHIC_FS];
  va_list argptr;
  va_start(argptr,s);
  vsprintf(t,s,argptr);
  va_end(argptr);
  /*  TODO draw to screen, wait for keypress and exit */
  /*  for now, just output to a file */
  {
    FILE *f=fopen("error.txt","w");
    fputs(t,f);
    fclose(f);
  }
  exit(1);
}

void resetlog() {
  FILE *f=fopen("log.txt","w");
  fclose(f);
}

void logprintf(char *s,...) {
  static char t[1000];
  FILE *f;
  va_list argptr;
  va_start(argptr,s);
  vsprintf(t,s,argptr);
  va_end(argptr);
  f=fopen("log.txt","a");
  fprintf(f,"%s",t);
  fclose(f);
}

double gettime() {
  struct timeval t;
  gettimeofday(&t,NULL);
  return t.tv_sec+t.tv_usec/1000000.;
}

static void split(char *s,char *t,char *u) {
  char *q=strchr(s,'=');
  if(!q) {
    t[0]=u[0]=0;
    return;
  }
  while(isspace(*s)) s++;
  while(*s!='=' && !isspace(*s)) *t++=*s++;
  s=q+1;
  while(isspace(*s)) s++;
  while(!isspace(*s) && *s!='%') *u++=*s++;
  *t=*u=0;
}

static Uint32 parsecolour(char *s) {
  Uint32 v=0;
  int i;
  for(i=0;s[i];i++) if(s[i]>='0' && s[i]<='9') v=v*10+s[i]-'0';
  else if(!strcmp(s,"BLACK")) return BLACK32;
  else if(!strcmp(s,"GRAY")) return GRAY32;
  else if(!strcmp(s,"GRAYBLUE")) return GRAYBLUE32;
  else if(!strcmp(s,"WHITE")) return WHITE32;
  else if(!strcmp(s,"GREEN")) return GREEN32;
  else if(!strcmp(s,"YELLOW")) return YELLOW32;
  else if(!strcmp(s,"RED")) return RED32;
  else if(!strcmp(s,"DARKRED")) return DARKRED32;
  else if(!strcmp(s,"DARKERRED")) return DARKERRED32;
  else if(!strcmp(s,"DARKBLUE")) return DARKBLUE32;
  else if(!strcmp(s,"BLUE")) return BLUE32;
  else error("illegal colour in definition file: %s\n",s);
  return (Uint32)v;
}

static void initinifile() {
  FILE *f=fopen("puzzle.ini","r");
	int maks;
	if(!f) return;
  while(fgets(s,GRAPHIC_FS,f)) if(s[0]!='%') {
    split(s,t,u);
    if(!strcmp(t,"x")) resx=strtol(u,0,10);
    else if(!strcmp(t,"y")) resy=strtol(u,0,10);
    else if(!strcmp(t,"fullscreen")) fullscreen=strtol(u,0,10);
    else if(!strcmp(t,"aspectratio")) aspectratio=strtod(u,0);
    else if(!strcmp(t,"thick")) thick=strtol(u,0,10);
    else if(!strcmp(t,"thin")) thin=strtol(u,0,10);
    else if(!strcmp(t,"edgethick")) edgethick=strtol(u,0,10);
    else if(!strcmp(t,"edgethick2")) edgethick2=strtol(u,0,10);
    /*  game specific other options */
    else if(!strcmp(t,"heyawakethick")) heyawakethick=strtol(u,0,10);
    else if(!strcmp(t,"slitherlinkthick")) slitherlinkthick=strtol(u,0,10);
    /*  control schemes */
    else if(!strcmp(t,"controlscheme_nurikabe")) controlscheme_nurikabe=strtol(u,0,10);
    else if(!strcmp(t,"controlscheme_akari")) controlscheme_akari=strtol(u,0,10);
    else if(!strcmp(t,"controlscheme_heyawake")) controlscheme_heyawake=strtol(u,0,10);
    else if(!strcmp(t,"controlscheme_hitori")) controlscheme_hitori=strtol(u,0,10);
    else if(!strcmp(t,"controlscheme_picross")) controlscheme_picross=strtol(u,0,10);
    else if(!strcmp(t,"controlscheme_slitherlink")) controlscheme_slitherlink=strtol(u,0,10);
    else if(!strcmp(t,"controlscheme_mine")) controlscheme_mine=strtol(u,0,10);
    else if(!strcmp(t,"controlscheme_kuromasu")) controlscheme_kuromasu=strtol(u,0,10);
    /*  colour options */
    else if(!strcmp(t,"unfilledcol")) unfilledcol=parsecolour(u);
    else if(!strcmp(t,"mustprocesscol")) mustprocesscol=parsecolour(u);
    else if(!strcmp(t,"filledcol")) filledcol=parsecolour(u);
    else if(!strcmp(t,"filled2col")) filled2col=parsecolour(u);
    else if(!strcmp(t,"blankcol")) blankcol=parsecolour(u);
    else if(!strcmp(t,"okcol")) okcol=parsecolour(u);
    else if(!strcmp(t,"almostokcol")) almostokcol=parsecolour(u);
    else if(!strcmp(t,"errorcol")) errorcol=parsecolour(u);
    else if(!strcmp(t,"darkerrorcol")) darkerrorcol=parsecolour(u);
    else if(!strcmp(t,"darkererrorcol")) darkererrorcol=parsecolour(u);
    else if(!strcmp(t,"lightcol")) lightcol=parsecolour(u);
    else if(!strcmp(t,"edgecol")) edgecol=parsecolour(u);
		/* path */
		else if(!strcmp(t,"puzzlepath")) {
			maks=strlen(u);
			if(maks>MAXPATH-1) maks=MAXPATH-1;
			strncpy(puzzlepath,u,maks);
			puzzlepath[maks]=0;
		}
  }
  fclose(f);
}

/*  for qsort */

int compi(const void *A,const void *B) {
  const int *a=A,*b=B;
  if(*a<*b) return -1;
  if(*a>*b) return 1;
  return 0;
}

static void updatevideoinfo() {
  /*  NB! This is part of the crazy fix for converting the routines to
      32-bit, without changing the memory access code. all scalars are
      simply divided by 4 to accomodate uint32 */
  pitch=screen->pitch>>2;
  bpp=screen->format->BytesPerPixel;
  /*  now that we have bpp: set up function pointers to graphic primitives */
  if(bpp!=4) error("wrong bpp: %d, only 4 supported)\n",bpp);
  bpp=1;
  pixels=screen->pixels;
}

static void initvideo() {
  /*  settings taken from tile world. what is SDL_ANYFORMAT? */
  if(SDL_Init(SDL_INIT_VIDEO)<0) exit(1);
  videoflags=SDL_SWSURFACE | SDL_ANYFORMAT;
  videoflags|=SDL_RESIZABLE;
  if(fullscreen) videoflags|=SDL_FULLSCREEN;
  if(!(screen=SDL_SetVideoMode(resx,resy,GRAPHIC_BPP,videoflags))) exit(1);
  updatevideoinfo();
}

void initgr() {
	gameinfo[0]=0;
  initinifile();
  initvideo();
  memset(keys,0,sizeof(keys));
  memset(mousebuttons,0,sizeof(mousebuttons));
  if(!(font=sdl_font_load("font.bmp"))) error("error loading font.");
}

void shutdowngr() {
  sdl_font_free(font);
  SDL_Quit();
}

void drawpixel32(int x,int y,Uint32 col) {
  Uint32 *p=pixels+y*pitch+x;
  *p=col;
}

/*  draw a horizontal line 1 pixel thick */
void drawhorizontalline32(int x1,int x2,int y,Uint32 col) {
  int w=x2-x1+1,i;
  Uint32 *p=pixels+y*pitch+x1;
  for(i=0;i<w;i++) p[i]=col;
}

/*  draw a vertical line 1 pixel thick */
void drawverticalline32(int x,int y1,int y2,Uint32 col) {
  int i;
  Uint32 *p=pixels+x+y1*pitch;
  for(i=y1;i<=y2;i++) *p=col,p+=pitch;
}

/*  draw a rectangle */
void drawrectangle32(int x1,int y1,int x2,int y2,Uint32 col) {
  int i,w=x2-x1+1,j;
  Uint32 *p=pixels+x1+y1*pitch;
  /*  TODO investigate performance for small widths */
  for(i=y1;i<=y2;i++) {
    for(j=0;j<w;j++) p[j]=col;
    p+=pitch;
  }
}

/*  draw a filled cell in cell x,y with custom border */
void drawsolidcell32w(int u,int v,Uint32 col,int left,int up,int right,int down) {
  drawrectangle32(startx+u*width+left,starty+v*height+up,
    startx+(u+1)*width-1-right,starty+(v+1)*height-1-down,col);
}

/*  draw a filled cell in cell x,y */
void drawsolidcell32(int u,int v,Uint32 col) {
  drawsolidcell32w(u,v,col,thick,thick,0,0);
}

void drawcross(int u,int v,Uint32 bk,Uint32 col) {
	int i,j,basex=startx+u*width,basey=starty+v*height;
	int w=width<height?width:height;
	drawsolidcell32(u,v,bk);
	for(i=thick+2,j=w-3;i<w-2;i++,j--) {
		drawpixel32(basex+i,basey+i,col);
		drawpixel32(basex+i-1,basey+i,col);
		drawpixel32(basex+i+1,basey+i,col);
		drawpixel32(basex+i,basey+i-1,col);
		drawpixel32(basex+i,basey+i+1,col);
		drawpixel32(basex+i,basey+j,col);
		drawpixel32(basex+i-1,basey+j,col);
		drawpixel32(basex+i+1,basey+j,col);
		drawpixel32(basex+i,basey+j-1,col);
		drawpixel32(basex+i,basey+j+1,col);
	}
}

/*  draw a number in cell x,y with custom border */
void drawnumbercell32w(int u,int v,int num,Uint32 col,Uint32 col2,Uint32 b,int left,int up,int right,int down) {
  drawrectangle32(startx+u*width+left,starty+v*height+up,
    startx+(u+1)*width-1-right,starty+(v+1)*height-1-down,b);
  sdl_font_printf(screen,font,startx+u*width+left+1,starty+v*height+up+1,col,
    col2,"%d",num);
}

/*  draw a number in cell x,y */
void drawnumbercell32(int u,int v,int num,Uint32 col,Uint32 col2,Uint32 b) {
  drawnumbercell32w(u,v,num,col,col2,b,thick,thick,0,0);
}

/*  draw a circular disc in cell x,y with radius r */
void drawdisc(int u,int v,double r,Uint32 col,Uint32 b) {
  double cx=thick+(width-thick-1)*.5,cy=thick+(height-thick-1)*.5;
  double dx,dy;
  int i,j;
  Uint32 *p;
  r*=r;
  for(j=thick;j<height;j++) {
    p=pixels+(u*width+startx)+(v*height+starty+j)*pitch;
    for(i=thick;i<width;i++) {
      dx=i-cx; dy=j-cy;
      p[i]=dx*dx+dy*dy<=r?col:b;
    }
  }
}

/*  outercol: colour of disc border
    innercol: colour of disc interior
    b: background colour */
void drawhollowdisc(int u,int v,double outer,double inner,Uint32 outercol,Uint32 innercol,Uint32 b) {
  double cx=thick+(width-thick-1)*.5,cy=thick+(height-thick-1)*.5;
  double dx,dy,dd;
  int i,j;
  Uint32 *p;
  inner*=inner;
  outer*=outer;
  for(j=thick;j<height;j++) {
    p=pixels+(u*width+startx)+(v*height+starty+j)*pitch;
    for(i=thick;i<width;i++) {
      dx=i-cx; dy=j-cy; dd=dx*dx+dy*dy;
      if(dd>outer) p[i]=b;
      else if(dd>inner) p[i]=outercol;
      else p[i]=innercol;
    }
  }
}

/*  clear the screen to one colour */
void clear32(Uint32 col) {
  int j,i;
  Uint32 *p;
  for(j=0;j<resy;j++) {
    p=pixels+j*pitch;
    for(i=0;i<resx;i++) p[i]=col;
  }
	/* the next line should not be there, updaterect is the caller's
	   responsibility */
/*  SDL_UpdateRect(screen,0,0,resx,resy);*/
}

/*  determine the size of each cell */
void updatescale(int resx,int resy,int x,int y,int thick) {
  width=(resx-thick-1)/x;
  height=(resy-thick-1)/y;
  if((int)(width/aspectratio)<height) height=(int)(width/aspectratio);
  if((int)(width/aspectratio)>height) width=(int)(height*aspectratio);
}

/*   return cell coordinates underneath mouse coordinates */
void getcell(int mousex,int mousey,int *cellx,int *celly) {
  *cellx=*celly=-1;
  if(mousex<startx || mousey<starty) return;
  /*  TODO exit if mouse pointer is to the right/below */
  /*  TODO exit if mouse pointer is exactly on the grid */
  *cellx=(mousex-startx)/width;
  *celly=(mousey-starty)/height;
}

/*  return cell border underneath mouse coordinates */
/*  celld: dimension (0:horizontal, 1:vertical, -1:error) */
void getborder(int mousex,int mousey,int *celld,int *cellx,int *celly) {
  int modx,mody,distup,distdown,distleft,distright;
  *celld=*cellx=*celly=-1;
  if(mousex<startx || mousey<starty) return;
  modx=(mousex-startx)%width;
  mody=(mousey-starty)%height;
  /*  reject mini-rectangle which is the intersection of grid lines */
  if(modx<thick && mody<thick) return;
  *cellx=(mousex-startx)/width;
  *celly=(mousey-starty)/height;
  /*  if mod is inside thick, immediately accept */
  if(modx<thick) { *celld=1; return; }
  if(mody<thick) { *celld=0; return; }
  distleft=modx-thick;
  distup=mody-thick;
  distright=width-modx-1;
  distdown=height-mody-1;
  if(distleft<=distup && distleft<=distdown && distleft<=distright) { *celld=1; return; }
  if(distup<=distleft && distup<=distdown && distup<=distright) { *celld=0; return; }
  if(distright<=distup && distright<=distdown && distright<=distleft) { *celld=1; (*cellx)++; return; }
  if(distdown<=distleft && distdown<=distup && distdown<=distright) { *celld=0; (*celly)++; return; }
}

int manhattandist(int x1,int y1,int x2,int y2) {
  int dx=x1-x2,dy=y1-y2;
  if(dx<0) dx=-dx;
  if(dy<0) dy=-dy;
  return dx+dy;
}

/*  refresh the screen under given cell coordinates */
void sdlrefreshcell(int cellx,int celly) {
  SDL_UpdateRect(screen,startx+cellx*width,starty+celly*height,width,height);
}

/*  rudimentary stack routines */
#define MAXSTACK 1000000
static int stack[MAXSTACK];
static int stackpos;

void stackpush(int val) {
  if(stackpos==MAXSTACK) error("attempt to push on a full stack!\n");
  stack[stackpos++]=val;
}

/*  return true if stack is empty */
int stackempty() {
  return !stackpos;
}

int stackpop() {
  if(!stackpos) error("attempt to pop from an empty stack!\n");
  return stack[--stackpos];
}

int getstackpos() {
  return stackpos;
}

void setstackpos(int sp) {
  stackpos=sp;
}

/*  loop until an event happens, and return it
    0-511:    keydown + id
    512-1023: keyup + id
    1024:    press mouse button (+ global variables)
    2048:    release mouse button (+ global variables)
    65536:    resize event
    131072:   close program event
*/

int event_mousebutton;
int event_mousex;
int event_mousey;
int event_mousefromx;
int event_mousefromy;
static SDL_Event event;

int getbuttonnumber(int ix) {
  if(ix==SDL_BUTTON_LEFT) return 0;
  else if(ix==SDL_BUTTON_RIGHT) return 1;
  else if(ix==SDL_BUTTON_MIDDLE) return 2;
  else if(ix==SDL_BUTTON_WHEELDOWN) return 3;
  else if(ix==SDL_BUTTON_WHEELUP) return 4;
/*  error("wrong mouse button: %d\n",ix);*/
  return 5;
}

int getevent() {
	Uint32 start=SDL_GetTicks(),cur;
	while(!SDL_PollEvent(&event)) {
		cur=SDL_GetTicks();
		if(cur-start>9) return EVENT_NOEVENT;
		SDL_Delay(10);
	}
	switch(event.type) {
	case SDL_VIDEORESIZE:
		screen=SDL_SetVideoMode(resx=event.resize.w,resy=event.resize.h,GRAPHIC_BPP,videoflags);
		if(!screen) exit(1);
		updatevideoinfo();
		return EVENT_RESIZE;
	case SDL_KEYDOWN:
		keys[event.key.keysym.sym]=1;
		return event.key.keysym.sym+EVENT_KEYDOWN;
	case SDL_KEYUP:
		keys[event.key.keysym.sym]=0;
		return event.key.keysym.sym+EVENT_KEYUP;
	case SDL_MOUSEBUTTONDOWN:
		event_mousebutton=event.button.button;
		mousebuttons[getbuttonnumber(event_mousebutton)]=1;
		event_mousex=event.button.x;
		event_mousey=event.button.y;
		return EVENT_MOUSEDOWN;
	case SDL_MOUSEBUTTONUP:
		event_mousebutton=event.button.button;
		mousebuttons[getbuttonnumber(event_mousebutton)]=0;
		event_mousex=event.button.x;
		event_mousey=event.button.y;
		return EVENT_MOUSEUP;
	case SDL_MOUSEMOTION:
		event_mousex=event.motion.x;
		event_mousey=event.motion.y;
		event_mousefromx=event.motion.x-event.motion.xrel;
		event_mousefromy=event.motion.y-event.motion.yrel;
		return EVENT_MOUSEMOTION;
	case SDL_QUIT:
		return EVENT_QUIT;
	}
  return EVENT_NOEVENT;
}

/* convert difficulty to number */
int getnumericdiff(const char *s) {
	if(!strcmp(s,"Trivial")) return 0;
	if(!strcmp(s,"Very easy")) return 1;
	if(!strcmp(s,"Easy")) return 2;
	if(!strcmp(s,"Easy+")) return 3;
	if(!strcmp(s,"Medium-")) return 4;
	if(!strcmp(s,"Medium")) return 5;
	if(!strcmp(s,"Hard-")) return 6;
	if(!strcmp(s,"Hard")) return 7;
	if(!strcmp(s,"Very hard")) return 8;
	if(!strcmp(s,"Extreme")) return 9;
	if(!strcmp(s,"Extra")) return 9;
	if(!strcmp(s,"Super hard")) return 9;
	if(!strcmp(s,"Near-impossible")) return 10;
	return -1;
}

/* scoring: timer and clicks */
static Uint32 starttime;	/* start time (ticks) */
int numclicks;	/* number of clicks */
int usedundo;	/* 1: undo was used */
int usedhint;	/* 1: hints were used */
int normalmove; /* 1: did normal moves */
int timespent; /* time spent for game in hundredths, or 0 if not won */

void resetscore() {
	starttime=SDL_GetTicks();
	numclicks=usedundo=usedhint=normalmove=0;
	timespent=0;
}

void displayscore(int x,int y) {
	Uint32 cur=(SDL_GetTicks()-starttime)/10;
	int buffer=4;
	int texty1=starty-font->height-buffer,texty2=texty1-font->height;
	static char s[65536],t[65536];
	sprintf(t,"Clicks: %d",numclicks);
	if(cur<6000) sprintf(s,"Time: %d:%02d",cur/100,cur%100);
	else if(cur<6000*60) sprintf(s,"Time: %d.%02d:%02d",cur/6000,cur/100%60,cur%100);
	else sprintf(s,"Time: %d.%02d.%02d:%02d",cur/360000,cur/6000%60,cur/100%60,cur%100);
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
	drawrectangle32(startx,texty2,resx-1,starty-buffer-1,WHITE32);
	sdl_font_printf(screen,font,startx,texty2,BLACK32,GRAY32,"%s",gameinfo);
	sdl_font_printf(screen,font,startx,texty1,BLACK32,GRAY32,"%s",s);
	sdl_font_printf(screen,font,startx+200,texty1,BLACK32,GRAY32,"%s",t);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	SDL_UpdateRect(screen,startx,texty2,resx-startx,font->height*2);
}

/* calculate final time for game */
void finalizetime() {
	timespent=(SDL_GetTicks()-starttime)/10;
}

/*  allow the application to refresh */
/*  return 1 if puzzle game needs to redraw the screen */
int dummyevent() {
  int res=SDL_PollEvent(&event),ret=0;
  if(res) {
    if(event.type==SDL_VIDEORESIZE) {
      screen=SDL_SetVideoMode(resx=event.resize.w,resy=event.resize.h,GRAPHIC_BPP,videoflags);
      if(!screen) exit(1);
      updatevideoinfo();
      ret=1;
    } else if(event.type==SDL_QUIT) {
      shutdowngr();
      exit(0);
    } else SDL_PushEvent(&event);
  }
  return ret;
}

/*  wait for a keypress or mouse key press */
/*  TODO resizing will not force redraw while we're here */
void anykeypress(void (*drawgrid)()) {
  int ievent;
  while(1) {
    ievent=getevent();
    if(ievent==EVENT_RESIZE && drawgrid!=0) (*drawgrid)();
    if(ievent>=EVENT_KEYDOWN && ievent<EVENT_KEYUP) break;
    if(ievent==EVENT_MOUSEDOWN) break;
    if(ievent==EVENT_QUIT) {
      /*  push back quit event  */
      SDL_PushEvent(&event);
      break;
    }
  }
}

/* TODO make a more advanced messagebox with support for multiple lines */
/* wait=1: wait for input, 0:display message and quit */
#define RAMME 16
#define SDL_FONT_S 65536
void messagebox(int wait,char *fmt,...) {
	static char t[SDL_FONT_S];
	int w,i,j,x,y,h=font->height,R=RAMME+RAMME;
	Uint32 *backup=0,*p;
	va_list argptr;
	va_start(argptr,fmt);
	vsprintf(t,fmt,argptr);
	va_end(argptr);
	w=sdl_font_width(font,t);
	if(wait) backup=(Uint32 *)malloc((w+R)*(h+R)*4);
	x=(resx-w-R)/2;
	y=(resy-font->height-R)/2;
	if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
	if(wait) for(i=0;i<font->height+R;i++) {
		p=pixels+(i+y)*pitch+x;
		for(j=0;j<w+R;j++) backup[(w+R)*i+j]=p[j];
	}
	/*  green interior */
	drawrectangle32(x,y,x+w+R-1,y+h+R-1,GREEN32);
	/*  big white outline */
	drawrectangle32(x,y,x+w+R-1,y+3,WHITE32);
	drawrectangle32(x,y+h+R-4,x+w+R-1,y+h+R-1,WHITE32);
	drawrectangle32(x,y,x+3,y+h+R-1,WHITE32);
	drawrectangle32(x+w+R-3,y,x+w+R-1,y+h+R-1,WHITE32);
	/*  tiny black outline */
	drawhorizontalline32(x,x+w+R-1,y,BLACK32);
	drawhorizontalline32(x,x+w+R-1,y+h+R-1,BLACK32);
	drawverticalline32(x,y,y+h+R-1,BLACK32);
	drawverticalline32(x+w+R-1,y,y+h+R-1,BLACK32);
	sdl_font_printf(screen,font,x+RAMME,y+RAMME,WHITE32,WHITE32,t);
	if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	SDL_UpdateRect(screen,x,y,w+R,h+R);
	if(wait) {
		anykeypress(0);
		if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
		for(i=0;i<font->height+R;i++) {
			p=pixels+(i+y)*pitch+x;
			for(j=0;j<w+R;j++) p[j]=backup[(w+R)*i+j];
		}
		if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		SDL_UpdateRect(screen,x,y,w+R,h+R);
		free(backup);
	}
}

/* launch game! */

static char *getid(char *s) {
  int i;
  for(i=strlen(s)-1;i>=0 && s[i]!='/' && s[i]!='\\';--i);
  return s+i+1;
}

void launch(char *s,int autosolve) {
  char *t=getid(s);
	setstackpos(0);
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
