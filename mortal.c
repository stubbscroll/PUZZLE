#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "SDL/SDL.h"
#include "puzzle.h"

/* max map size */
#define MAXS 2002

/* max string length */
#define MAXSTR 1024
static int x,y;                 /* map size */
static char difficulty[MAXSTR]; /* string holding the difficulty */

#define UNFILLED 0
#define FILLED 1
#define WALL 2

static char mn[MAXS][MAXS];       /* indicate empty or filled cells */
static char m[2][MAXS][MAXS];     /* 1 edge, 0 no edge, [0]:down, [1]:right */
static char touched[MAXS][MAXS];  /* 1 if cell is changed and need to be redrawn */

/* warning, this level has a different "stack" than the other puzzles */
int pathx[MAXS*MAXS],pathy[MAXS*MAXS];
int pathlen;

static int convchar(char *s,int *i) {
	char c=s[(*i)++];
	if(!c) error("string in level definition ended prematurely.");
	else if(c=='.') return UNFILLED;
	else if(c=='#') return WALL;
	error("invalid character %c in level definition.",c);
	return 0;
}

static void loadpuzzle(char *path) {
	static char s[MAXSTR];
	FILE *f=fopen(path,"r");
	int z=0,ln=0,i,j;
	if(!f) error("couldn't open the file %s\n",path);
	gameinfo[0]=0;
	while(fgets(s,MAXSTR,f)) if(s[0]!='%') {
		switch(z) {
		case 1:
			strcpy(difficulty,s);
		case 0:
			z++;
			break;
		case 2:
			if(2!=sscanf(s,"%d %d",&x,&y)) error("expected x and y size");
			z++;
			break;
		case 3:
			for(i=j=0;j<x;j++) mn[j][ln]=convchar(s,&i);
			ln++;
		}
	} else if(!gameinfo[0]) strcpy(gameinfo,s+2);
	if(fclose(f)) error("error reading file");
	startx=10,starty=(int)(font->height*2.5);
	for(i=0;i<x;i++) for(j=0;j<y;j++) touched[i][j]=m[0][i][j]=m[1][i][j]=0;
}

static void updateedge(int u,int v,Uint32 col) {
  int x1=thick+(width-thick-edgethick)/2,y1=thick+(height-thick-edgethick)/2,x2=x1+edgethick,y2=y1+edgethick;
  if(u && m[0][u-1][v]) drawrectangle32(startx+u*width,starty+v*height+y1,startx+u*width+x2-1,starty+v*height+y2-1,col);
  if(v && m[1][u][v-1]) drawrectangle32(startx+u*width+x1,starty+v*height,startx+u*width+x2-1,starty+v*width+y2-1,col);
  if(m[0][u][v]) drawrectangle32(startx+u*width+x1,starty+v*height+y1,startx+(u+1)*width-1,starty+v*height+y2-1,col);
  if(m[1][u][v]) drawrectangle32(startx+u*width+x1,starty+v*height+y1,startx+u*width+x2-1,starty+(v+1)*height-1,col);
}

static void updatecell(int u,int v) {
  int w=width<height?width:height;
  if(thick) {
    /* left edge */
    drawrectangle32(startx+u*width,starty+v*height,startx+u*width+thick-1,starty+(v+1)*height-1,BLACK32);
    /* upper edge */
    drawrectangle32(startx+u*width,starty+v*height,startx+(u+1)*width-1,starty+v*height+thick-1,BLACK32);
  }
  /* unfilled cell */
  if(mn[u][v]==UNFILLED) drawsolidcell32(u,v,unfilledcol);
  /* unfilled cell */
  else if(mn[u][v]==FILLED) drawsolidcell32(u,v,blankcol);
  /* wall */
	else if(mn[u][v]==WALL) drawsolidcell32(u,v,filledcol);
	/* if we're at start or end of path, draw white circle */
	if(pathlen) {
		if((pathx[0]==u && pathy[0]==v) || (pathx[pathlen-1]==u && pathy[pathlen-1]==v))
			drawhollowdisc(u,v,(w-thick)*0.42,(w-thick)*0.35,filledcol,blankcol,blankcol);
	}
  /*  draw the edges */
  updateedge(u,v,edgecol);
}

static void drawgrid() {
  int i,j;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  clear32(WHITE32);
  updatescale(resx-startx,resy-starty,x,y,thick);
  if(thick) {
    for(i=0;i<=y;i++) for(j=0;j<thick;j++) drawhorizontalline32(startx,startx+width*x+thick-1,starty+i*height+j,BLACK32);
    for(i=0;i<=x;i++) drawrectangle32(startx+width*i,starty,startx+i*width+thick-1,starty+y*height+thick-1,BLACK32);
  }
  for(i=0;i<x;i++) for(j=0;j<y;j++) updatecell(i,j);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
}

/* (x1,y1) and (x2,y2) must be adjacent */
static void setedge(int x1,int y1,int x2,int y2,int val) {
	int dx=x2-x1,dy=y2-y1;
	if(dx*dx+dy*dy>1) error("logical error, bad edge");
	touched[x1][y1]=touched[x2][y2]=1;
	if(dx>0) m[0][x1][y1]=val;
	else if(dx<0) m[0][x2][y2]=val;
	else if(dy>0) m[1][x1][y1]=val;
	else if(dy<0) m[1][x2][y2]=val;
}

/* do move bookkeeping, including putting it on the ¨"stack" */
static void domove(int cellx,int celly) {
	int dx=0,dy=0,px,py,x2,y2,x3,y3;
	if(!pathlen) {
		/* different handling of first move */
		if(mn[cellx][celly]==WALL) error("logical error, tried to start on wall");
		mn[cellx][celly]=FILLED;
		pathx[0]=cellx;
		pathy[0]=celly;
		pathlen++;
		touched[cellx][celly]=1;
	} else {
		px=pathx[pathlen-1];
		py=pathy[pathlen-1];
		if(px==cellx && py==celly) error("logical error, moved to same cell");
		if(px!=cellx && py!=celly) error("logical error, cannot determine direction");
		if(px<cellx) dx=1;
		else if(px>cellx) dx=-1;
		if(py<celly) dy=1;
		else if(py>celly) dy=-1;
		if(mn[px+dx][py+dy]!=UNFILLED) error("logical error, cannot move in direction");
		/* continue in direction */
		x2=px; y2=py;
		while(1) {
			x3=x2+dx; y3=y2+dy;
			if(x3<0 || y3<0 || x3>=x || y3>=y || mn[x3][y3]!=UNFILLED) break;
			mn[x3][y3]=FILLED;
			setedge(x2,y2,x3,y3,1);
			x2=x3; y2=y3;
		}
		pathx[pathlen]=x2; pathy[pathlen++]=y2;
	}
}

static void partialredraw() {
  int i,j;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(touched[i][j]) updatecell(i,j);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(touched[i][j]) {
    sdlrefreshcell(i,j);
    touched[i][j]=0;
  }
}

static void updatetoscreen(int visible) {
  if(visible) partialredraw();
}

static void undo(int visible) {
	int x1,y1,x2,y2,dx=0,dy=0;
	if(pathlen==1) {
		/* remove circle only */
		x2=pathx[0]; y2=pathy[0];
		touched[x2][y2]=1;
		mn[x2][y2]=UNFILLED;
		pathlen--;
		updatetoscreen(visible);
	} else if(pathlen) {
		x1=pathx[pathlen-2];
		y1=pathy[pathlen-2];
		x2=pathx[pathlen-1];
		y2=pathy[pathlen-1];
		if(x1<x2) dx=1;
		else if(x1>x2) dx=-1;
		else if(y1<y2) dy=1;
		else if(y1>y2) dy=-1;
		while(x1!=x2 || y1!=y2) {
			setedge(x1,y1,x1+dx,y1+dy,0);
			x1+=dx; y1+=dy;
			mn[x1][y1]=UNFILLED;
		}
		pathlen--;
		updatetoscreen(visible);
	}
}

/* 0 if unfinished, 1 if solved
   unsolvable not detected for now */
static int verifyboard() {
	int i,j;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]==UNFILLED) return 0;
	return 1;
}

static int canmove(int dx,int dy) {
	int x1,y1;
	if(!pathlen) return 0;
	x1=pathx[pathlen-1]+dx;
	y1=pathy[pathlen-1]+dy;
	logprintf("try %d %d\n",x1,y1);
	return x1>=0 && x1<x && y1>=0 && y1<y && mn[x1][y1]==UNFILLED;
}

static void processkeydown(int key) {
	int x2=0,y2=0,up=0;
	if(pathlen) x2=pathx[pathlen-1],y2=pathy[pathlen-1];
  if(key==undokey) undo(1),usedundo=1;
  else if(key==hintkey || key==SDLK_j)  {
		messagebox(1,"Hints not yet supported for this puzzle.");
	} else if(key==SDLK_DOWN) {
		if(canmove(0,1)) domove(x2,y2+1),up=normalmove=1,numclicks++;
	} else if(key==SDLK_UP) {
		if(canmove(0,-1)) domove(x2,y2-1),up=normalmove=1,numclicks++;
	} else if(key==SDLK_LEFT) {
		if(canmove(-1,0)) domove(x2-1,y2),up=normalmove=1,numclicks++;
	} else if(key==SDLK_RIGHT) {
		if(canmove(1,0)) domove(x2+1,y2),up=normalmove=1,numclicks++;
	}
  if(up) updatetoscreen(1);
}

static void processmousedown() {
  int cellx,celly,up=0,dx=0,dy=0,x2,y2;
  getcell(event_mousex,event_mousey,&cellx,&celly);
  if(cellx<0 || celly<0 || cellx>=x || celly>=y) return;
	if(event_mousebutton==SDL_BUTTON_LEFT) {
		if(!pathlen) {
			if(mn[cellx][celly]!=UNFILLED) return;
			domove(cellx,celly); up=1; normalmove=1; numclicks++;
		} else {
			/* reject no move */
			if(cellx==pathx[pathlen-1] && celly==pathy[pathlen-1]) return;
			/* reject un-straight line */
			if(cellx!=pathx[pathlen-1] && celly!=pathy[pathlen-1]) return;
			/* check if we can move in desired direction */
			x2=pathx[pathlen-1];
			y2=pathy[pathlen-1];
			if(x2<cellx) dx=1;
			else if(x2>cellx) dx=-1;
			else if(y2<celly) dy=1;
			else if(y2>celly) dy=-1;
			if(mn[x2+dx][y2+dy]!=UNFILLED) return;
			/* ok */
			domove(cellx,celly); up=1; normalmove=1; numclicks++;
		}
	}
  if(up) updatetoscreen(1);
}

void mortal(char *path,int solve) {
  int event;
  loadpuzzle(path);
	pathlen=0;
  drawgrid();
	/* TODO no autosolver for now */
  if(solve) return;
	resetscore();
  do {
    event=getevent();
		displayscore(x,y);
    switch(event) {
    case EVENT_RESIZE:
      drawgrid();
    case EVENT_NOEVENT:
      break;
    case EVENT_MOUSEDOWN:
      processmousedown();
      if(verifyboard()>0) {
				finalizetime(); displayscore(x,y);
        messagebox(1,"You are winner!");
        return;
      }
      break;
    default:
      /*  catch intervals of values here */
      if(event>=EVENT_KEYDOWN && event<EVENT_KEYUP) {
        processkeydown(event-EVENT_KEYDOWN);
        if(verifyboard()>0) {
					finalizetime(); displayscore(x,y);
          messagebox(1,"You are winner!");
          return;
        }
      }
    }
  } while(event!=EVENT_QUIT && !keys[SDLK_ESCAPE]);
}
