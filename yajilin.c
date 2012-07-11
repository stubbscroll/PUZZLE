#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "SDL/SDL.h"
#include "puzzle.h"

/*  max map size */
#define MAXS 128
/*  max string length */
#define MAXSTR 1024
/*  total rendered grid size is (x+mx) * (y+my) */
static int x,y;                 /*  map size */
static char difficulty[MAXSTR]; /*  string holding the difficulty */

/*  -1:  no number
    0-  numbered cell */
static int mn[MAXS][MAXS];
/*  direction (only if mn>0), uses dx directions (right,down,left,up) */
static int md[MAXS][MAXS];
/*  indicator of what's filled in. 1:filled in, 0:not filled in
    [0] edge going right
    [1] edge going down
    [2] blocked cell
    0,1 cannot coexist with 2.
*/
static int m[MAXS][MAXS][3];
static char touched[MAXS][MAXS];  /*  1 if cell is changed and need to be redrawn */
/* on arrow cells, number of blocks past its arrow until next arrow!
   desired number is difference between current arrow and next arrow */
/* colouring:
   0: black: less blocks than indicated number
   1: yellow: correct number of blocks, blank cells exist (missing lines)
	 2: green: correct number of blocks, no blank cells
	 3: red: more blocks than indicated number
	 a cell is blank if it has no square and less than two edges.
	 st[][] is guaranteed to correctly reflect the board at all times!
*/
static int st[MAXS][MAXS];

/* for each cell, keep a list of all arrows pointing to it */
static int ptrx[MAXS][MAXS][MAXS];
static int ptry[MAXS][MAXS][MAXS];
static int ptrn[MAXS][MAXS];
static int ptrnum[MAXS][MAXS];

/*  move queue for hint system */
#define MAXMQ MAXS*MAXS*4
static int mq[MAXMQ];
static int mqs,mqe;

/*  from a cell, edge directions */
static int ex[4]={0,0,-1,0},ey[4]={0,0,0,-1},ed[4]={0,1,0,1};

static int convchar(char c) {
  if(c=='.') return -1;
  else if(c>='0' && c<='9') return c-48;
  else if(c>='a' && c<='z') return c-'a'+10;
  else if(c>='A' && c<='Z') return c-'A'+36;
  error("invalid character %c in level definition.",c);
  return 0;
}

static void genptr(int atx,int aty,int dx,int dy) {
	int cellx=atx,celly=aty,type=md[cellx][celly];
	atx+=dx;
	aty+=dy;
	while(atx>=0 && atx<x && aty>=0 && aty<y) {
		if(mn[atx][aty]>-1 && md[atx][aty]==type) {
			ptrnum[cellx][celly]=mn[cellx][celly]-mn[atx][aty];
			return;
		}
		if(mn[atx][aty]<0) {
			ptrx[atx][aty][ptrn[atx][aty]]=cellx;
			ptry[atx][aty][ptrn[atx][aty]++]=celly;
		}
		atx+=dx;
		aty+=dy;
	}
	ptrnum[cellx][celly]=mn[cellx][celly];
}

static void loadpuzzle(char *path) {
  static char s[MAXSTR];
  FILE *f=fopen(path,"r");
  int z=0,ln=0,i,j;
  if(!f) error("couldn't open the file %s\n",path);
  while(fgets(s,MAXSTR,f)) if(s[0]!='%') {
    switch(z) {
    case 1:
      strcpy(difficulty,s);
    case 0:
      z++;
      break;
    case 2:
      sscanf(s,"%d %d",&x,&y);
      z++;
      break;
    case 3:
      for(i=j=0;j<x;j++) {
        mn[j][ln]=convchar(s[i++]);
        md[j][ln]=0;
        if(s[i]=='v') md[j][ln]=1;
        else if(s[i]=='<') md[j][ln]=2;
        else if(s[i]=='^') md[j][ln]=3;
        i++;
      }
      ln++;
    }
  }
  fclose(f);
  startx=10,starty=30;
  mqs=mqe=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    st[i][j]=touched[i][j]=0;
    m[i][j][0]=m[i][j][1]=m[i][j][2]=0;
		ptrn[i][j]=0;
  }
	/* for each cell, generate list of arrows */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]>-1) {
		if(md[i][j]==3) genptr(i,j,0,-1);
		else if(md[i][j]==1) genptr(i,j,0,1);
		else if(md[i][j]==2) genptr(i,j,-1,0);
		else if(!md[i][j]) genptr(i,j,1,0);
	}
}

static int hasneighbouringblocked(int u,int v) {
  int d,x1,y1;
  for(d=0;d<4;d++) {
    x1=dx[d],y1=dy[d];
    if(x1>=0 && y1>=0 && x1<x && y1<x && m[x1][y1][2]) return 1;
  }
  return 0;
}

static void updateedge(int u,int v,Uint32 col) {
  int x1=thick+(width-thick-edgethick)/2,y1=thick+(height-thick-edgethick)/2,x2=x1+edgethick,y2=y1+edgethick;
	int isleft=u && m[u-1][v][0],isup=v && m[u][v-1][1],isright=m[u][v][0],isdown=m[u][v][1];
  if(isleft) drawrectangle32(startx+u*width+thick,starty+v*height+y1,startx+u*width+x2-1,starty+v*height+y2-1,col);
  if(isup) drawrectangle32(startx+u*width+x1,starty+v*height+thick,startx+u*width+x2-1,starty+v*width+y2-1,col);
  if(isright) drawrectangle32(startx+u*width+x1,starty+v*height+y1,startx+(u+1)*width-1,starty+v*height+y2-1,col);
  if(isdown) drawrectangle32(startx+u*width+x1,starty+v*height+y1,startx+u*width+x2-1,starty+(v+1)*height-1,col);
}

/*  draw number and arrow */
/*  TODO draw nice arrows with scaled arrowhead */
static void drawarrownumber(int u,int v,int num,int dir,Uint32 col,Uint32 bk) {
  char s[16]={0,0};
  sprintf(s,"%d",num);
  if(dir&1) {
    /*  vertical */
    drawrectangle32(startx+(u+1)*width-6,starty+v*height+thick+2,startx+(u+1)*width-4,starty+(v+1)*height-3,col);
    if(dir&2) {
      /*  up arrow */
      drawpixel32(startx+(u+1)*width-5,starty+v*height+thick+1,col);
      drawhorizontalline32(startx+(u+1)*width-7,startx+(u+1)*width-3,starty+v*height+thick+3,col);
      drawhorizontalline32(startx+(u+1)*width-8,startx+(u+1)*width-2,starty+v*height+thick+4,col);
    } else {
      /*  down arrow */
      drawpixel32(startx+(u+1)*width-5,starty+(v+1)*height-2,col);
      drawhorizontalline32(startx+(u+1)*width-7,startx+(u+1)*width-3,starty+(v+1)*height-4,col);
      drawhorizontalline32(startx+(u+1)*width-8,startx+(u+1)*width-2,starty+(v+1)*height-5,col);
    }
    sdl_font_printf(screen,font,startx+u*width+thick+2,starty+v*height+thick+2,col,col,"%s",s);
  } else {
    /*  horizontal */
    drawrectangle32(startx+u*width+thick+2,starty+v*height+thick+3,startx+(u+1)*width-3,starty+v*height+thick+5,col);
    if(dir&2) {
      /*  left arrow */
      drawpixel32(startx+u*width+thick+1,starty+v*height+thick+4,col);
      drawrectangle32(startx+u*width+thick+3,starty+v*height+thick+2,startx+u*width+thick+3,starty+v*height+thick+6,col);
      drawrectangle32(startx+u*width+thick+4,starty+v*height+thick+1,startx+u*width+thick+4,starty+v*height+thick+7,col);
    } else {
      /*  right arrow */
      drawpixel32(startx+(u+1)*width-2,starty+v*height+thick+4,col);
      drawrectangle32(startx+(u+1)*width-4,starty+v*height+thick+2,startx+(u+1)*width-4,starty+v*height+thick+6,col);
      drawrectangle32(startx+(u+1)*width-5,starty+v*height+thick+1,startx+(u+1)*width-5,starty+v*height+thick+7,col);
    }
    sdl_font_printf(screen,font,startx+u*width+thick+2,starty+v*height+thick+12,col,col,"%s",s);
  }
}

static void updatecellcol(int u,int v,Uint32 edgecol,Uint32 blankcol2) {
  Uint32 bkcol;
  if(thick) {
    /*  left edge */
    drawrectangle32(startx+u*width,starty+v*height,startx+u*width+thick-1,starty+(v+1)*height-1,BLACK32);
    /*  upper edge */
    drawrectangle32(startx+u*width,starty+v*height,startx+(u+1)*width-1,starty+v*height+thick-1,BLACK32);
  }
  if(mn[u][v]>-1) {
    bkcol=blankcol;
		if(st[u][v]==1) bkcol=almostokcol;
		else if(st[u][v]==2) bkcol=okcol;
		else if(st[u][v]==3) bkcol=errorcol;
		drawrectangle32(startx+u*width+thick,starty+v*height+thick,startx+(u+1)*width-1,starty+(v+1)*height-1,bkcol);
    drawarrownumber(u,v,mn[u][v],md[u][v],BLACK32,bkcol);
  } else {
		drawrectangle32(startx+u*width+thick,starty+v*height+thick,startx+(u+1)*width-1,starty+(v+1)*height-1,blankcol);
    /*  non-number cell: draw either blocked or edges */
    if(m[u][v][2]) drawsolidcell32(u,v,hasneighbouringblocked(u,v)?errorcol:filledcol);
    else updateedge(u,v,edgecol);
  }
}

static void updatecell(int u,int v) {
  updatecellcol(u,v,edgecol,blankcol);
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

/* update background colour for arrow cell */
static void updatearrow(int cellx,int celly) {
	int newcol,count=0,empty=0,dirs,type=md[cellx][celly];
	int dx=0,dy=0,atx=cellx,aty=celly;
	if(!type) dx=1;
	else if(type==1) dy=1;
	else if(type==2) dx=-1;
	else if(type==3) dy=-1;
	atx+=dx; aty+=dy;
	while(atx>=0 && atx<x && aty>=0 && aty<y) {
		if(mn[atx][aty]>-1 && md[atx][aty]==type) break;
		if(mn[atx][aty]<0) {
			if(m[atx][aty][2]) count++;
			else if(!empty) {
				/* count number of edges */
				dirs=0;
				if(m[atx][aty][0]) dirs++; /* right */
				if(m[atx][aty][1]) dirs++; /* down */
				if(atx && m[atx-1][aty][0]) dirs++; /* left */
				if(aty && m[atx][aty-1][1]) dirs++; /* up */
				if(dirs<2) empty=1,logprintf("  [emptyness because of %d %d (%d %d %d %d)]\n",atx,aty,
					m[atx][aty][0],m[atx][aty][1],m[atx-1][aty][0],m[atx][aty-1][1]);
			}
		}
		atx+=dx;
		aty+=dy;
	}
	logprintf("arrow at %d %d needs %d, found %d empty %d\n",cellx,celly,ptrnum[cellx][celly],count,empty);
	if(count<ptrnum[cellx][celly]) newcol=0;
	else if(count>ptrnum[cellx][celly]) newcol=3;
	else newcol=empty?1:2;
	if(st[cellx][celly]!=newcol) {
		st[cellx][celly]=newcol;
		touched[cellx][celly]=1;
	}
}

/* also check if arrows change colour */
static void applymove(int cellx,int celly,int celld,int val) {
	int i,atx,aty;
  m[cellx][celly][celld]=val;
  touched[cellx][celly]=1;
  if(celld==1) touched[cellx][celly+1]=1;
  else if(!celld) touched[cellx+1][celly]=1;
	/* recheck all arrows affected by this cell */
	for(i=0;i<ptrn[cellx][celly];i++)
		updatearrow(ptrx[cellx][celly][i],ptry[cellx][celly][i]);
	/* if edge: check other cell */
	if(celld<2) {
		atx=cellx;
		aty=celly;
		if(celld) aty++;
		else atx++;
		for(i=0;i<ptrn[atx][aty];i++)
			updatearrow(ptrx[atx][aty][i],ptry[atx][aty][i]);
	}
}

static void undo(int visible) {
  if(!stackempty()) {
    int val=stackpop(),celld=stackpop(),celly=stackpop(),cellx=stackpop();
    applymove(cellx,celly,celld,val);
    updatetoscreen(visible);
  }
}

/*  do move bookkeeping, including putting it on the stack */
static void domove(int cellx,int celly,int celld,int val) {
  if(val==m[cellx][celly][celld]) error("logical error, tried to set cell to existing value");
  stackpush(cellx); stackpush(celly); stackpush(celld); stackpush(m[cellx][celly][celld]);
  applymove(cellx,celly,celld,val);
}

/*  BFS stuff */
static uchar visit[MAXS][MAXS];
static int qs,qe,q[MAXS*MAXS*3];

static void initbfs() {
  qs=qe=0;
  memset(visit,0,sizeof(visit));
}

static int togglecell(int val) {
  return val^1;
}

static void processmousedown() {
  int cellx,celly,d,x2,y2,new=-1,e;
  getcell(event_mousex,event_mousey,&cellx,&celly);
  if(cellx<0 || celly<0 || cellx>=x || celly>=y || mn[cellx][celly]>-1) return;
  if(event_mousebutton==SDL_BUTTON_RIGHT) new=1;
  else if(event_mousebutton==SDL_BUTTON_MIDDLE) new=0;
  if(new>-1 && m[cellx][celly][2]!=new) {
    domove(cellx,celly,2,new);
    for(d=0;d<4;d++) {
      x2=cellx+ex[d],y2=celly+ey[d],e=ed[d];
      if(x2>=0 && x2<x && y2>=0 && y2<y && mn[x2][y2]<0 && m[x2][y2][e]) {
        domove(x2,y2,e,0);
      }
    }
    updatetoscreen(1);
  }
}

static void processmousemotion() {
  int x1,y1,x2,y2,t,d;
  if(mousebuttons[0]) {
    getcell(event_mousefromx,event_mousefromy,&x1,&y1);
    getcell(event_mousex,event_mousey,&x2,&y2);
    if(x1<0 || y1<0 || x1>=x || y1>=y) return;
    if(x2<0 || y2<0 || x2>=x || y2>=y) return;
    if(manhattandist(x1,y1,x2,y2)==1) {
      if(x1>x2) t=x1,x1=x2,x2=t;
      if(y1>y2) t=y1,y1=y2,y2=t;
      if(x1==x2) d=1;
      else d=0;
      if(mn[x1][y1]>-1 || mn[x2][y2]>-1 || m[x1][y1][2] || m[x2][y2][2]) return;
      domove(x1,y1,d,togglecell(m[x1][y1][d]));
      updatetoscreen(1);
    }
  }
}

/* hint:
   level 4: for each interval (line between two successive arrows),
	 try all combinations of blocks (don't check extremely large cases)
	 level 5: same as above, but recurse
*/

static void processkeydown(int key) {
  int res;
  if(key==undokey) undo(1);
}

static void autosolver(char *s) {
  int res;
  double start=gettime(),end;
  logprintf("%s: ",s);
/*  while(hint()>0) executemovequeue();
  res=verifyboard();*/
	res=0;
  end=gettime()-start;
  if(end<0) end=0;
  logprintf("[%.3fs] ",end);
  if(res==-1) logprintf("Solver reached illegal state!\n");
  else if(!res) logprintf("Not solved\n");
  else logprintf("Solved!\n");
}

void yajilin(char *path,int solve) {
  int event,i,j;
  loadpuzzle(path);
  initbfs();
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]>-1) updatearrow(i,j);
  drawgrid();
  if(solve) { autosolver(path); return; }
  do {
    event=getevent();
    switch(event) {
    case EVENT_RESIZE:
      drawgrid();
    case EVENT_NOEVENT:
      break;
    case EVENT_MOUSEDOWN:
      processmousedown();
/*      if(verifyboard()>0) {
        messagebox("You are winner!");
        return;
      }*/
      break;
    case EVENT_MOUSEMOTION:
      if(mousebuttons[0]) {
        processmousemotion();
/*        if(verifyboard()>0) {
          messagebox("You are winner!");
          return;
        }*/
      }
      break;
    default:
      /*  catch intervals of values here */
      if(event>=EVENT_KEYDOWN && event<EVENT_KEYUP) {
        processkeydown(event-EVENT_KEYDOWN);
/*        if(verifyboard()>0) {
          messagebox("You are winner!");
          return;
        }*/
      }
    }
  } while(event!=EVENT_QUIT && !keys[SDLK_ESCAPE]);
}
