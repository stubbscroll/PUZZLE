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
    [2] blocked cell:
		    0 if no block
				1 if blocked
				2 if X
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
	else if(c=='*') return -2;
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
  int z=0,ln=0,i,j,r;
	memset(m,0,sizeof(m));
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
				r=convchar(s[i++]);
				if(r<-1) mn[j][ln]=-1,m[j][ln][2]=2;
        else mn[j][ln]=r;
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
    x1=u+dx[d],y1=v+dy[d];
    if(x1>=0 && y1>=0 && x1<x && y1<x && m[x1][y1][2]==1) return 1;
  }
  return 0;
}

/* return number of edges in cell */
static int degree(int i,int j) {
	int count;
	if(i<0 || j<0 || i>=x || j>=y) return 0;
	count=(m[i][j][0]>0)+(m[i][j][1]>0);
	if(i && m[i-1][j][0]) count++;
	if(j && m[i][j-1][1]) count++;
	return count;
}

/* check if we can create edge from u,v in direction d */
/* this function assumes that u,v is empty with degree<2 */
/* return 1 if edge already exists */
static int legaledge(int u,int v,int d) {
	int x2=u+dx[d],y2=v+dy[d],x3=u+ex[d],y3=v+ey[d];
	if(x2<0 || y2<0 || x2>=x || y2>=y) return 0;
	if(mn[x2][y2]>-1 || m[x2][y2][2]) return 0;
	if(m[x3][y3][ed[d]]) return 1;
	return degree(x2,y2)<2;
}

/* return 1 if cell is empty (no blocked, no X, no arrow, but edges are
   allowed */
static int isempty(int u,int v) {
	if(u<0 || v<0 || u>=x || v>=y) return 0;
	if(mn[u][v]>-1) return 0;
	if(m[u][v][2]) return 0;
	return 1;
}

static int countlegaledges(int u,int v) {
	int d,count=0;
	for(d=0;d<4;d++) if(isempty(u+dx[d],v+dy[d]) && legaledge(u,v,d)) count++;
	return count;
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
    if(m[u][v][2]==1)
			drawsolidcell32(u,v,hasneighbouringblocked(u,v)?errorcol:filledcol);
		else if(m[u][v][2]==2) drawcross(u,v,filled2col);
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
			if(m[atx][aty][2]==1) count++;
			else if(!empty) {
				/* count number of edges */
				dirs=0;
				if(m[atx][aty][0]) dirs++; /* right */
				if(m[atx][aty][1]) dirs++; /* down */
				if(atx && m[atx-1][aty][0]) dirs++; /* left */
				if(aty && m[atx][aty-1][1]) dirs++; /* up */
				if(dirs<2) empty=1;
			}
		}
		atx+=dx;
		aty+=dy;
	}
	if(count<ptrnum[cellx][celly]) newcol=0;
	else if(count>ptrnum[cellx][celly]) newcol=3;
	else newcol=empty?1:2;
	if(st[cellx][celly]!=newcol) {
		st[cellx][celly]=newcol;
		touched[cellx][celly]=1;
	}
}

static void applymove(int cellx,int celly,int celld,int val) {
	int i,atx,aty,x2,y2;
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
	} else {
		for(i=0;i<4;i++) {
			x2=cellx+dx[i]; y2=celly+dy[i];
			if(x2>=0 && y2>=0 && x2<x && y2<y) touched[x2][y2]=1;
		}
	}
}

/*  BFS stuff */
static uchar visit[MAXS][MAXS];
static int qs,qe,q[MAXS*MAXS*2];

static void cleanupbfs() {
  while(qe) visit[q[qe-2]][q[qe-1]]=0,qe-=2;
  qs=0;
}

static void followedge(int sx,int sy) {
  int cx,cy;
  if(visit[sx][sy]) return;
  visit[sx][sy]=1;
  q[qe++]=sx; q[qe++]=sy;
  while(qs<qe) {
    cx=q[qs++],cy=q[qs++];
		if(m[cx][cy][0] && !visit[cx+1][cy]) {
			q[qe++]=cx+1; q[qe++]=cy; visit[cx+1][cy]=1;
		}
		if(m[cx][cy][1] && !visit[cx][cy+1]) {
			q[qe++]=cx; q[qe++]=cy+1; visit[cx][cy+1]=1;
		}
		if(cx && m[cx-1][cy][0] && !visit[cx-1][cy]) {
			q[qe++]=cx-1; q[qe++]=cy; visit[cx-1][cy]=1;
		}
		if(cy && m[cx][cy-1][1] && !visit[cx][cy-1]) {
			q[qe++]=cx; q[qe++]=cy-1; visit[cx][cy-1]=1;
		}
  }
}

/* 0: unfinished, 1: finished, -1: error */
static int verifyboard() {
	int i,j,incomplete=0,count,x2,y2,loop=0,loose=0;
	/* check for illegal stuff: adjacent blocked */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j][2]==1 && hasneighbouringblocked(i,j)) return -1;
	/* check edges */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(!m[i][j][2] && mn[i][j]<0) {
		count=degree(i,j);
		if(count>2) return -1;
		if(count<2) incomplete=1;
	}
	/* for each arrow, check number of squares */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]>-1) {
		count=0;
		x2=i; y2=j;
		x2+=dx[md[i][j]]; y2+=dy[md[i][j]];
		while(x2>=0 && x2<x && y2>=0 && y2<y) {
			if(mn[x2][y2]>-1 && md[x2][y2]==md[i][j]) break;
			if(mn[x2][y2]<0 && m[x2][y2][2]==1) count++;
			x2+=dx[md[i][j]]; y2+=dy[md[i][j]];
		}
		if(count>ptrnum[i][j]) return -1;
		if(count<ptrnum[i][j]) incomplete=1;
	}
	/* check all edges:
		 closed loop + other edges => illegal 
	   closed loop + no missing edges => solved
		 all other cases: incomplete */
	/* first, bfs from all endpoints */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]<0 && !m[i][j][2] && !visit[i][j] && degree(i,j)==1) {
		loose=incomplete=1;
		followedge(i,j);
	}
	/* find closed loops */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]<0 && !m[i][j][2] && !visit[i][j] && degree(i,j)==2) {
		loop=1;
		followedge(i,j);
		goto doneclose;
	}
doneclose:
	if(loop && loose) {
		cleanupbfs();
		return -1;
	}
	if(loop) for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]<0 && !m[i][j][0] && !visit[i][j] && degree(i,j)) {
		cleanupbfs();
		return -1;
	}
	cleanupbfs();
	/* TODO check more advanced stuff later */
	return 1-incomplete;
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

/*  start of hint system! */

static void addmovetoqueue(int cellx,int celly,int celld,int val) {
  mq[mqe++]=cellx; mq[mqe++]=celly; mq[mqe++]=celld; mq[mqe++]=val;
  if(mqe==MAXMQ) mqe=0;
}

static int movequeueisempty() {
  return mqs==mqe;
}

/*  return 0:no moves in queue, 1:move successfully executed */
static int executeonemovefromqueue(int visible) {
loop:
  if(movequeueisempty()) return 0;
  /*  the hint system can produce some moves twice, don't redo moves */
  if(m[mq[mqs]][mq[mqs+1]][mq[mqs+2]]==mq[mqs+3]) {
    mqs+=4;
    if(mqs==MAXMQ) mqs=0;
    goto loop;
  }
  domove(mq[mqs],mq[mqs+1],mq[mqs+2],mq[mqs+3]);
  updatetoscreen(visible);
  mqs+=4;
  if(mqs==MAXMQ) mqs=0;
  return 1;
}

static void executemovequeue() {
  while(executeonemovefromqueue(1)) if(dummyevent()) drawgrid();
}

/* hint:
   level 1: check forced 1 (1 cell), 2 (3 cells), 3 (5 cells)
   level 1: if there are two blocked with one blank inbetween,
	          that cell must have edge through it
   level 4: for each interval (line between two successive arrows),
	 try all combinations of blocks (don't check extremely large cases)
	 level 5: same as above, but recurse
*/

/* remove edges going into u,v, making room for blocked */
static void removeedges(int i,int j) {
	if(m[i][j][0]) addmovetoqueue(i,j,0,0);
	if(m[i][j][1]) addmovetoqueue(i,j,1,0);
	if(i && m[i-1][j][0]) addmovetoqueue(i-1,j,0,0);
	if(j && m[i][j-1][1]) addmovetoqueue(i,j-1,1,0);
}

/* if two arrows pointing the same way has a gap of one cell between
   them: then the gap must contain either an edge going though (if
	 arrows have the same number) or blocked (number difference==1) */
static int level1gapone() {
	int i,j,x2,y2,x3,y3,ok=0,d;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]>-1) {
		x2=i+dx[md[i][j]]; y2=j+dy[md[i][j]];
		x3=x2+dx[md[i][j]]; y3=y2+dy[md[i][j]];
		if(x2<0 || y2<0 || x2>=x || y2>=y) continue;
		if(mn[x2][y2]>-1) continue;
		if(m[x2][y2][2]) continue;
		if(x3>=0 && x3<x && y3>=0 && y3<y && (mn[x3][y3]<0 || md[x3][y3]!=md[i][j])) continue;
		if(ptrnum[i][j]==1 && !m[i][j][2]) {
			removeedges(x2,y2);
			ok=1;
			addmovetoqueue(x2,y2,2,1);
		} else {
			if(m[x2][y2][2]) addmovetoqueue(x2,y2,2,0),ok=1;
			d=md[i][j]&1;
			if(d) {
				if(!m[x2][y2][0]) addmovetoqueue(x2,y2,0,1),ok=1;
				if(!m[x2-1][y2][0]) addmovetoqueue(x2-1,y2,0,1),ok=1;
			} else {
				if(!m[x2][y2][1]) addmovetoqueue(x2,y2,1,1),ok=1;
				if(!m[x2][y2-1][1]) addmovetoqueue(x2,y2-1,1,1),ok=1;
			}
		}
	}
	return ok;
}

/* if there is a one-cell gap between two blocked or a blocked and an
   arrow/X, then the gap must contain edge */
static int level1blockedgap() {
	int i,j,blocked,ok=0;
	/* downward */
	for(i=1;i<x-1;i++) for(j=0;j<y-2;j++) {
		blocked=0;
		if(m[i][j][2]==1) blocked++;
		if(m[i][j+2][2]==1) blocked++;
		if(mn[i][j+1]>-1) continue;
		if(!blocked) continue;
		if(blocked<2 && mn[i][j]<0 && m[i][j][2]<2 && mn[i][j+2]<0 && m[i][j+2][2]<2) continue;
		if(!m[i][j+1][0]) addmovetoqueue(i,j+1,0,1),ok=1;
		if(!m[i-1][j+1][0]) addmovetoqueue(i-1,j+1,0,1),ok=1;
	}
	/* sideways */
	for(i=0;i<x-2;i++) for(j=1;j<y-1;j++) {
		blocked=0;
		if(m[i][j][2]==1) blocked++;
		if(m[i+2][j][2]==1) blocked++;
		if(mn[i+1][j]>-1) continue;
		if(!blocked) continue;
		if(blocked<2 && mn[i][j]<0 && m[i][j][2]<2 && mn[i+2][j]<0 && m[i+2][j][2]<2) continue;
		if(!m[i+1][j][1]) addmovetoqueue(i+1,j,1,1),ok=1;
		if(!m[i+1][j-1][1]) addmovetoqueue(i+1,j-1,1,1),ok=1;
	}
	return ok;
}

/* if an empty cell borders a blocked (or already one edge pointing into it)
   and there are only two possible ways to make an edge, then fill in these
   edges */
static int level1twoways() {
	int i,j,count,d,ok=0;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]<0 && !m[i][j][2] && degree(i,j)<2 && (hasneighbouringblocked(i,j) || degree(i,j)==1)) {
		count=0;
		for(d=0;d<4;d++) if(legaledge(i,j,d)) count++;
		if(count==2) {
			ok=1;
			for(d=0;d<4;d++) if(legaledge(i,j,d) && !m[i+ex[d]][j+ey[d]][ed[d]])
				addmovetoqueue(i+ex[d],j+ey[d],ed[d],1),ok=1;
		}
	}
	return ok;
}

/* if an empty cell has only one legal edge, then it must be blocked */
static int level1oneedge() {
	int i,j,d,ok=0,count;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]<0 && !m[i][j][2]) {
		for(count=d=0;d<4;d++) if(legaledge(i,j,d)) count++;
		if(count==1) for(d=0;d<4;d++) if(legaledge(i,j,d) && !m[i+ex[d]][j+ey[d]][ed[d]])
			addmovetoqueue(i,j,2,1),ok=1;
	}
	return ok;
}

/* if there are two adjacent empty where each has two legal edges,
   fill in the edges */
static int level1tunnel() {
	int i,j,d,ok=0;
	/* vertical */
	for(i=0;i<x;i++) for(j=0;j<y-1;j++) if(isempty(i,j) && isempty(i,j+1)) {
		if(countlegaledges(i,j)==2 && countlegaledges(i,j+1)==2) {
			for(d=0;d<4;d++) {
				if(legaledge(i,j,d) && !m[i+ex[d]][j+ey[d]][ed[d]]) addmovetoqueue(i+ex[d],j+ey[d],ed[d],1),ok=1;
				if(legaledge(i,j+1,d) && !m[i+ex[d]][j+ey[d]+1][ed[d]]) addmovetoqueue(i+ex[d],j+ey[d]+1,ed[d],1),ok=1;
			}
		}
	}
	/* horizontal */
	for(i=0;i<x-1;i++) for(j=0;j<y;j++) if(isempty(i,j) && isempty(i+1,j)) {
		if(countlegaledges(i+1,j)==2 && countlegaledges(i,j)==2) {
			for(d=0;d<4;d++) {
				if(legaledge(i,j,d) && !m[i+ex[d]][j+ey[d]][ed[d]]) addmovetoqueue(i+ex[d],j+ey[d],ed[d],1),ok=1;
				if(legaledge(i+1,j,d) && !m[i+ex[d]+1][j+ey[d]][ed[d]]) addmovetoqueue(i+ex[d]+1,j+ey[d],ed[d],1),ok=1;
			}
		}
	}
	return ok;
}

static int level1isolated() {
	int i,j,count,d,ok=0;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(isempty(i,j) && !degree(i,j)) {
		for(count=d=0;d<4;d++) if(!legaledge(i,j,d)) count++;
		if(count>3) addmovetoqueue(i,j,2,1),ok=1;
	}
	return ok;
}

static int level1hint() {
	if(level1gapone()) return 1;
	if(level1blockedgap()) return 1;
	if(level1twoways()) return 1;
	if(level1oneedge()) return 1;
	if(level1tunnel()) return 1;
	if(level1isolated()) return 1;
  return 0;
}

/* if there is only one way to fill in blocked in an interval between
   two arrows pointing the same way, do it */
static int level2uniqueblocked() {
	int i,j,x2,y2,rem,atx,aty,rem2,wrong;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]>-1 && (rem=ptrnum[i][j])) {
		/* find first cell beyond segment */
		x2=i+dx[md[i][j]]; y2=j+dy[md[i][j]];
		while(x2>=0 && x2<x && y2>=0 && y2<y) {
			if(mn[x2][y2]>-1 && md[x2][y2]==md[i][j]) break;
			if(m[x2][y2][2]==1) rem--;
			x2+=dx[md[i][j]]; y2+=dy[md[i][j]];
		}
		if(!rem) continue;
		/* fill in the correct way and check if the last block
		   is placed on the last eligible cell */
		atx=i+dx[md[i][j]]; aty=j+dy[md[i][j]];
		rem2=rem;
		while(rem) {
			if(mn[atx][aty]<0 && !degree(atx,aty) && !m[atx][aty][2] && !hasneighbouringblocked(atx,aty)) {
				visit[atx][aty]=1;
				rem--;
				atx+=dx[md[i][j]];
				aty+=dy[md[i][j]];
				if(!rem) break;
			}
			atx+=dx[md[i][j]];
			aty+=dy[md[i][j]];
		}
		/* traverse backwards and compare */
		rem=rem2;
		atx=x2-dx[md[i][j]];
		aty=y2-dy[md[i][j]];
		wrong=0;
		while(atx!=i || aty!=j) {
			if(mn[atx][aty]<0 && !degree(atx,aty) && !m[atx][aty][2] && !hasneighbouringblocked(atx,aty)) {
				if(!visit[atx][aty]) wrong=1;
				rem--;
				atx-=dx[md[i][j]];
				aty-=dy[md[i][j]];
				if(!rem) break;
			}
			atx-=dx[md[i][j]];
			aty-=dy[md[i][j]];
		}
		/* clean up visit array */
		atx=i; aty=j;
		while(atx!=x2 || aty!=y2) {
			visit[atx][aty]=0;
			atx+=dx[md[i][j]];
			aty+=dy[md[i][j]];
		}
		if(wrong) continue;
		/* ok, insert */
		atx=i+dx[md[i][j]]; aty=j+dy[md[i][j]];
		while(1) {
			if(mn[atx][aty]<0 && !degree(atx,aty) && !m[atx][aty][2] && !hasneighbouringblocked(atx,aty)) {
				addmovetoqueue(atx,aty,2,1);
				atx+=dx[md[i][j]];
				aty+=dy[md[i][j]];
				if(atx==x2 && aty==y2) break;
			}
			atx+=dx[md[i][j]];
			aty+=dy[md[i][j]];
			if(atx==x2 && aty==y2) break;
		}
		return 1;
	}
	return 0;
}

/* given an arrow which is fulfilled with blocked: if any cell only has
   two legal edges, the cell must be filled in this way */
static int level2donearrow() {
	int i,j,atx,aty,ok=0,d;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]>-1 && st[i][j]==1) {
		atx=i+dx[md[i][j]]; aty=j+dy[md[i][j]];
		while(atx>=0 && atx<x && aty>=0 && aty<y) {
			if(mn[atx][aty]>-1 && md[atx][aty]==md[i][j]) break;
			if(mn[atx][aty]>-1) goto inc;
			if(m[atx][aty][2]) goto inc;
			if(countlegaledges(atx,aty)==2) {
				for(d=0;d<4;d++) if(legaledge(atx,aty,d) && !m[atx+ex[d]][aty+ey[d]][ed[d]])
					ok=1,addmovetoqueue(atx+ex[d],aty+ey[d],ed[d],1);
			}
		inc:
			atx+=dx[md[i][j]]; aty+=dy[md[i][j]];
		}
		if(ok) return 1;
	}
	return 0;
}

static int level2hint() {
	if(level2uniqueblocked()) return 1;
	if(level2donearrow()) return 1;
  return 0;
}

/* if all possibilities of making an edge (except one) creates a loop
   which doesn't solve the level, then create the only edge that doesn't */
static int level3avoidloop() {
	int i,j,count,okdir,d;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(degree(i,j)==1) {
		okdir=-1;
		count=0;
		for(d=0;d<4;d++) if(legaledge(i,j,d) && !m[i+ex[d]][j+ey[d]][ed[d]]) {
			if(degree(i+dx[d],j+dy[d])==1) {
				m[i+ex[d]][j+ey[d]][ed[d]]=1;
				if(verifyboard()>-1) count++,okdir=d;
				m[i+ex[d]][j+ey[d]][ed[d]]=0;
			} else count++,okdir=d;
		}
		if(count==1) {
			addmovetoqueue(i+ex[okdir],j+ey[okdir],ed[okdir],1);
			return 1;
		}
	}
	return 0;
}

static int level3hint() {
	if(level3avoidloop()) return 1;
  return 0;
}

static int level4hint() {
  return 0;
}

static int level5hint() {
  return 0;
}

static int hint() {
  if(verifyboard()<0) return -1;
  if(level1hint()) return 1;
  if(level2hint()) return 1;
  if(level3hint()) return 1;
  if(level4hint()) return 1;
  if(level5hint()) return 1;
	return 0;
}

static Uint32 colarray[]={
  0x000055, 0x0000AA, 0x0000FF, 0x006D00, 0x0092AA, 0x920000, 0x496DAA
};
#define COLSIZE 7

static int forcefullredraw;

static void drawverify() {
  int i,j,k,col=0,qs2;
  if(forcefullredraw) drawgrid();
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  /*  first, search through all loose ends */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && degree(i,j)==1) {
    qs2=qs;
    followedge(i,j);
    for(k=qs2;k<qe;k+=2) updateedge(q[k],q[k+1],colarray[col]);
    col=(col+1)%COLSIZE;
  }
  /*  then, search through all unvisited closed loops */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && degree(i,j)==2) {
    qs2=qs;
    followedge(i,j);
    for(k=qs2;k<qe;k+=2) updateedge(q[k],q[k+1],colarray[col]);
    col=(col+1)%COLSIZE;
  }
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
  cleanupbfs();
}

static void showverify() {
  int i,j;
  forcefullredraw=0;
  drawverify();
  forcefullredraw=1;
  anykeypress(drawverify);
  /*  display normal level */
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) updatecell(i,j);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
}
#undef COLSIZE

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
  if(cellx<0 || celly<0 || cellx>=x || celly>=y || mn[cellx][celly]>-1 || m[cellx][celly][2]>1) return;
  if(event_mousebutton==SDL_BUTTON_RIGHT) new=1;
  else if(event_mousebutton==SDL_BUTTON_MIDDLE) new=0;
  if(new>-1 && m[cellx][celly][2]!=new) {
    domove(cellx,celly,2,new);
    for(d=0;d<4;d++) {
      x2=cellx+ex[d],y2=celly+ey[d],e=ed[d];
      if(x2>=0 && x2<x && y2>=0 && y2<y && mn[x2][y2]<0 && m[x2][y2][2]<2 && m[x2][y2][e]) {
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

static void processkeydown(int key) {
  int res;
  if(key==undokey) undo(1);
  else if(key==verifykey) showverify();
  else if(key==hintkey) {
    if(!executeonemovefromqueue(1)) {
      res=hint();
      if(res>0) executeonemovefromqueue(1);
      else if(!res) messagebox("Sorry, no moves found.");
      else messagebox("Sorry, hint will not work on an illegal board.");
    }
  } else if(key==SDLK_j) {  /* temporary: superhintkey */
    res=hint();
    if(res>0) {
      executemovequeue();
      while(hint()>0) executemovequeue();
      if(verifyboard()<1) messagebox("Sorry, no more moves found.");
    } else if(!res) messagebox("Sorry, no moves found.");
    else messagebox("Sorry, hint will not work on an illegal board.");
  }
}

static void autosolver(char *s) {
  int res;
  double start=gettime(),end;
  logprintf("%s: ",s);
  while(hint()>0) executemovequeue();
  res=verifyboard();
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
      if(verifyboard()>0) {
        messagebox("You are winner!");
        return;
      }
      break;
    case EVENT_MOUSEMOTION:
      if(mousebuttons[0]) {
        processmousemotion();
        if(verifyboard()>0) {
          messagebox("You are winner!");
          return;
        }
      }
      break;
    default:
      /*  catch intervals of values here */
      if(event>=EVENT_KEYDOWN && event<EVENT_KEYUP) {
        processkeydown(event-EVENT_KEYDOWN);
        if(verifyboard()>0) {
          messagebox("You are winner!");
          return;
        }
      }
    }
  } while(event!=EVENT_QUIT && !keys[SDLK_ESCAPE]);
}
