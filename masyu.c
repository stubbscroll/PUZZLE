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

/*  'o':  white disc
    '*':  black disc
    '.':  empty cell */
static int mn[MAXS][MAXS];
/*  indicate whether there is an edge going to the cell to its right, or below */
/*  0: no edge, 1: edge */
static int m[2][MAXS][MAXS];

static char touched[MAXS][MAXS];  /*  1 if cell is changed and need to be redrawn */

/*  move queue for hint system */
#define MAXMQ MAXS*MAXS*4
static int mq[MAXMQ];
static int mqs,mqe;

/*  from a cell, edge directions */
static int ex[4]={0,0,-1,0},ey[4]={0,0,0,-1},ed[4]={0,1,0,1};

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
      for(i=j=0;j<x;j++) mn[j][ln]=s[i++];
      ln++;
    }
  }
  fclose(f);
  startx=10,starty=30;
  mqs=mqe=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    touched[i][j]=0;
    m[0][i][j]=m[1][i][j]=0;
  }
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
    /*  left edge */
    drawrectangle32(startx+u*width,starty+v*height,startx+u*width+thick-1,starty+(v+1)*height-1,BLACK32);
    /*  upper edge */
    drawrectangle32(startx+u*width,starty+v*height,startx+(u+1)*width-1,starty+v*height+thick-1,BLACK32);
  }
  /*  empty cell */
  if(mn[u][v]=='.') drawsolidcell32(u,v,blankcol);
  /*  white circle */
  else if(mn[u][v]=='o') drawhollowdisc(u,v,(w-thick)*0.42,(w-thick)*0.35,filledcol,blankcol,blankcol);
  /*  black circle */
  else if(mn[u][v]=='*') drawdisc(u,v,(w-thick)*0.42,filledcol,blankcol);
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

/*  do move bookkeeping, including putting it on the stack */
static void domove(int celld,int cellx,int celly,int val) {
  if(val==m[celld][cellx][celly]) error("logical error, tried to set cell to existing value");
  stackpush(celld); stackpush(cellx); stackpush(celly); stackpush(m[celld][cellx][celly]);
  m[celld][cellx][celly]=val;
  touched[cellx][celly]=1;
  if(celld) touched[cellx][celly+1]=1;
  else touched[cellx+1][celly]=1;
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

static void updatetoscreen(int celld,int cellx,int celly,int visible) {
  if(visible) partialredraw();
}

static void undo(int visible) {
  if(!stackempty()) {
    int val=stackpop(),celly=stackpop(),cellx=stackpop(),celld=stackpop();
    m[celld][cellx][celly]=val;
    touched[cellx][celly]=1;
    if(celld) touched[cellx][celly+1]=1;
    else touched[cellx+1][celly]=1;
    updatetoscreen(celld,cellx,celly,visible);
  }
}

/*  start of hint system! */

static void addmovetoqueue(int celld,int cellx,int celly,int val) {
  mq[mqe++]=celld; mq[mqe++]=cellx; mq[mqe++]=celly; mq[mqe++]=val;
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
  updatetoscreen(mq[mqs],mq[mqs+1],mq[mqs+2],visible);
  mqs+=4;
  if(mqs==MAXMQ) mqs=0;
  return 1;
}

static void executemovequeue() {
  while(executeonemovefromqueue(1)) if(dummyevent()) drawgrid();
}

static void silentexecutemovequeue() {
  while(executeonemovefromqueue(0));
}

/*  return the number of edges adjacent to a cell */
static int numedges(int u,int v) {
  int count=0;
  if(u && m[0][u-1][v]) count++;
  if(v && m[1][u][v-1]) count++;
  if(m[0][u][v]) count++;
  if(m[1][u][v]) count++;
  return count;
}

/*  BFS stuff */
static uchar visit[MAXS][MAXS];
static int qs,qe,q[MAXS*MAXS*2];

static void initbfs() {
  qs=qe=0;
  memset(visit,0,sizeof(visit));
}

/*  type 0: search in "blank" area with numedges<2
    return: loose:  number of cells with numedges==1 visited */
static void genericbfs(int sx,int sy,int type,int *loose,int *dots) {
  int cx,cy,x2,y2,d,num;
  num=numedges(sx,sy);
  *loose=(num==1);
  *dots=(mn[sx][sy]!='.');
  if(type==0 && num>1) return;
  if(visit[sx][sy]) return;
  visit[sx][sy]=1;
  q[qe++]=sx; q[qe++]=sy;
  while(qs<qe) {
    cx=q[qs++]; cy=q[qs++];
    for(d=0;d<4;d++) {
      x2=cx+dx[d],y2=cy+dy[d];
      if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2]) continue;
      num=numedges(x2,y2);
      if(type==0 && num>1) continue;
      if(num==1) (*loose)++;
      q[qe++]=x2; q[qe++]=y2;
      visit[x2][y2]=1;
      if(mn[x2][y2]!='.') (*dots)++;
    }
  }
}

static void followedge(int sx,int sy) {
  int cx,cy,d2,x2,y2,d,prevx=-1,prevy=-1;
  if(visit[sx][sy]) return;
  visit[sx][sy]=1;
  q[qe++]=sx; q[qe++]=sy;
  while(qs<qe) {
    cx=q[qs++],cy=q[qs++];
    for(d=0;d<4;d++) {
      d2=ed[d],x2=cx+ex[d],y2=cy+ey[d];
      if(!m[d2][x2][y2]) continue;
      x2=cx+dx[d]; y2=cy+dy[d];
      if(x2==prevx && y2==prevy) continue;
      if(visit[x2][y2]) continue;
      q[qe++]=x2; q[qe++]=y2;
      visit[x2][y2]=1;
      prevx=cx; prevy=cy;
      break;
    }
  }
}

static void cleanupbfs() {
  while(qe) visit[q[qe-2]][q[qe-1]]=0,qe-=2;
  qs=0;
}

/*  return 1 if cell contains two corner edges */
static int iscorner(int u,int v) {
  if(m[0][u][v] && m[1][u][v]) return 1;
  if(u && m[0][u-1][v] && m[1][u][v]) return 1;
  if(v && m[1][u][v-1] && m[0][u][v]) return 1;
  if(u && v && m[0][u-1][v] && m[1][u][v-1]) return 1;
  return 0;
}

/*  return 1 if cell is the center of a straight edge of length 4 */
static int isstraight4(int u,int v) {
  if(u<2 || v<2 || u>x-3 || v>y-3) return 0;
  if(m[0][u-2][v] && m[0][u-1][v] && m[0][u][v] && m[0][u+1][v]) return 1;
  if(m[1][u][v-2] && m[1][u][v-1] && m[1][u][v] && m[1][u][v+1]) return 1;
  return 0;
}

/*  return 1 if the cell is the center of a straight edge of length 2 */
static int isstraight2(int u,int v) {
  if(u && m[0][u-1][v] && m[0][u][v]) return 1;
  if(v && m[1][u][v-1] && m[1][u][v]) return 1;
  return 0;
}

/*  check if there is a straight line of length 2 going through u,v with
    direction d */
static int isstraight2dir(int d,int u,int v) {
  if(!d && (u<1 || u>x-2)) return 0;
  if(d && (v<1 || v>y-2)) return 0;
  if(!d && m[d][u-1][v] && m[d][u][v]) return 1;
  if(d && m[d][u][v-1] && m[d][u][v]) return 1;
  return 0;
}

/*  check if there is a straight line of length 3 starting at u,v with
    direction d */
static int isstraight3dir(int d,int u,int v) {
  if(!d && u>x-4) return 0;
  if(d && v>y-4) return 0;
  if(!d && m[d][u][v] && m[d][u+1][v] && m[d][u+2][v]) return 1;
  if(d && m[d][u][v] && m[d][u][v+1] && m[d][u][v+2]) return 1;
  return 0;
}

/*  check if it is legal to put an edge in d,u,v
    0: edge is definitely illegal
    1: edge is possibly legal */
/*  return 0 if an edge in d,u,v causes at least one of:
    - a cell with >2 outgoing edges
    - a white dot having two corner edges
    - a black dot having two straight edges
    - edge goes out of board
    - edge causes a nearby black dot to have an outgoing edge of length 1 */
static int isedgeallowed(int d,int u,int v) {
  int bak;
  /*  out of board */
  if(u<0 || v<0) return 0;
  if(!d && u>=x-1) return 0;
  if(d && v>=y-1) return 0;
  /*  check if cell has >2 outgoing edges */
  if(numedges(u,v)>1) return 0;
  if(d && numedges(u,v+1)>1) return 0;
  if(!d && numedges(u+1,v)>1) return 0;
  /*  set edge */
  bak=m[d][u][v];
  m[d][u][v]=1;
  /*  check if the edge turns in a white dot */
  if(m[d][u][v]=='o' && iscorner(u,v)) { m[d][u][v]=bak; return 0; }
  if(d && v<y-1 && m[1][u][v+1]=='o' && iscorner(u,v+1)) { m[d][u][v]=bak; return 0; }
  if(!d && u<x-1 && m[0][u+1][v]=='o' && iscorner(u+1,v)) { m[d][u][v]=bak; return 0; }
  /*  check if the edge goes straight through a black dot */
  if(m[d][u][v]=='*' && isstraight2(u,v)) { m[d][u][v]=bak; return 0; }
  if(d && v<y-1 && m[1][u][v+1]=='o' && isstraight2(u,v+1)) { m[d][u][v]=bak; return 0; }
  if(!d && u<x-1 && m[0][u+1][v]=='o' && isstraight2(u+1,v)) { m[d][u][v]=bak; return 0; }
  /*  check if edge of length 4 goes through white dot */
  if(m[d][u][v]=='o' && isstraight4(u,v)) { m[d][u][v]=bak; return 0; }
  if(d && v<y-1 && m[1][u][v+1]=='o' && isstraight4(u,v+1)) { m[d][u][v]=bak; return 0; }
  if(!d && u<x-1 && m[0][u+1][v]=='o' && isstraight4(u+1,v)) { m[d][u][v]=bak; return 0; }
  /*  check if edge causes black dot to have edge of length 1 */
  if(d==0) {
    if(iscorner(u,v) && m[0][u+1][v]=='*') { m[d][u][v]=bak; return 0; }
    if(m[0][u][v]=='*' && iscorner(u+1,v)) { m[d][u][v]=bak; return 0; }
  } else {
    if(iscorner(u,v) && m[1][u][v+1]=='*') { m[d][u][v]=bak; return 0; }
    if(m[1][u][v]=='*' && iscorner(u,v+1)) { m[d][u][v]=bak; return 0; }
  }
  m[d][u][v]=bak;
  return 1;
}

/*  return -1 if illegal (unsolvable) board, 0 if unfinished, 1 if solved */
static int verifyboard() {
  int ok=1,i,j,num,d,d2,x2,y2,loops=0,paths=0,count,tot,qs2,k,dots;
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    /*  check for illegal state: cell with >2 edges */
    /*  also detect non-solved: loose edge and unfulfilled dot */
    num=numedges(i,j);
    if(num>2) return -1;
    if(num==1) ok=0;
    if(mn[i][j]!='.' && num<2) ok=0;
    /*  check for black dots with straight line */
    if(mn[i][j]=='*' && isstraight2(i,j)) return -1;
    /*  check for white line with corner */
    if(mn[i][j]=='o' && iscorner(i,j)) return -1;
    /*  check for white line without corner next to it */
    if(mn[i][j]=='o' && isstraight4(i,j)) return -1;
    /*  check for black dots with 1-line */
    if(mn[i][j]=='*') for(d=0;d<4;d++) {
      d2=ed[d]; x2=i+ex[d]; y2=j+ey[d];
      if(m[d2][x2][y2] && iscorner(i+dx[d],j+dy[d])) return -1;
    }
    /*  check for loose end with no legal moves */
    count=0;
    if(num==1) for(d=0;d<4;d++) {
      d2=ed[d]; x2=i+ex[d]; y2=j+ey[d];
      if(m[d2][x2][y2]) continue;
      if(isedgeallowed(d2,x2,y2)) break;
      else count++;
    }
    if(count==3) return -1;
  }
  /*  check for non-solving loop:
      - there exists a closed loop and:
        - additional edges in another loop
        - white/black dots with <2 edges */
  /*  first, get rid of open paths */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && numedges(i,j)==1) {
    ok=0;
    paths++;
    followedge(i,j);
  }
  /*  find closed paths */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && numedges(i,j)==2) {
    if(paths) { cleanupbfs(); return -1; }
    loops++;
    followedge(i,j);
    goto theend;
  }
theend:
  /*  fail if there still are unvisited edges */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && numedges(i,j)) {
    cleanupbfs();
    return -1;
  }
  cleanupbfs();
  /*  check for areas with odd number of loose ends */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && numedges(i,j)==1) {
    qs2=qs;
    genericbfs(i,j,0,&tot,&dots);
    if(tot&1) {
      cleanupbfs();
      return -1;
    }

    continue;
    /*  TODO commented out for now. C028 left has a situation which
        meets the requirements, but it has a white dot forcing a
        turn */
  /*  also check uniqueness! fail if there is an area with two adjacent loose
      ends, and if there are two adjacent cells near it. this is disallowed
      because otherwise the puzzle would have multiple solutions */
      /*  see bottom right of C028 */
    if(tot==2 && !dots) {
      for(k=qs2;k<qe;k+=2) if(numedges(q[k],q[k+1])==1) {
        x2=q[k],y2=q[k+1];
        if(x2<x-1 && numedges(x2+1,y2)==1) {
          if(y2 && numedges(x2,y2-1)==0 && numedges(x2+1,y2-1)==0) { cleanupbfs(); return -1; }
          if(y2<y-1 && numedges(x2,y2+1)==0 && numedges(x2+1,y2+1)==0) { cleanupbfs(); return -1; }
        }
        if(y2<y-1 && numedges(x2,y2+1)==1) {
          if(x2 && numedges(x2-1,y2)==0 && numedges(x2-1,y2+1)==0) { cleanupbfs(); return -1; }
          if(x2<x-1 && numedges(x2+1,y2)==0 && numedges(x2+1,y2+1)==0) { cleanupbfs(); return -1; }
        }
      }
    }
  }
  cleanupbfs();
  if(loops && !ok) return -1;
  return ok;
}

/*  if a black dot is near or 1 away from border: two edges must point away */
static int level1blacknearborder() {
  int i,j,ok=0;
  for(i=0;i<x;i++) for(j=0;j<2;j++) if(mn[i][j]=='*') {
    if(!m[1][i][j]) addmovetoqueue(1,i,j,1),ok=1;
    if(!m[1][i][j+1]) addmovetoqueue(1,i,j+1,1),ok=1;
  }
  for(i=0;i<x;i++) for(j=y-2;j<y;j++) if(mn[i][j]=='*') {
    if(!m[1][i][j-1]) addmovetoqueue(1,i,j-1,1),ok=1;
    if(!m[1][i][j-2]) addmovetoqueue(1,i,j-2,1),ok=1;
  }
  for(j=0;j<y;j++) for(i=0;i<2;i++) if(mn[i][j]=='*') {
    if(!m[0][i][j]) addmovetoqueue(0,i,j,1),ok=1;
    if(!m[0][i+1][j]) addmovetoqueue(0,i+1,j,1),ok=1;
  }
  for(j=0;j<y;j++) for(i=x-2;i<x;i++) if(mn[i][j]=='*') {
    if(!m[0][i-1][j]) addmovetoqueue(0,i-1,j,1),ok=1;
    if(!m[0][i-2][j]) addmovetoqueue(0,i-2,j,1),ok=1;
  }
  return ok;
}

/*  if a white dot is near border, edge must go through it, parallel with border */
static int level1whitenearborder() {
  int i,j,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j+=y-1) if(mn[i][j]=='o') {
    if(!m[0][i-1][j]) addmovetoqueue(0,i-1,j,1),ok=1;
    if(!m[0][i][j]) addmovetoqueue(0,i,j,1),ok=1;
  }
  for(i=0;i<x;i+=x-1) for(j=0;j<y;j++) if(mn[i][j]=='o') {
    if(!m[1][i][j-1]) addmovetoqueue(1,i,j-1,1),ok=1;
    if(!m[1][i][j]) addmovetoqueue(1,i,j,1),ok=1;
  }
  return ok;
}

/*  if there are three whites in a row, draw three parallel edges through them */
static int level1threewhite() {
  int i,j,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    if(i<x-2 && mn[i][j]=='o' && mn[i+1][j]=='o' && mn[i+2][j]=='o') {
      if(!m[1][i][j-1]) addmovetoqueue(1,i,j-1,1),ok=1;
      if(!m[1][i][j]) addmovetoqueue(1,i,j,1),ok=1;
      if(!m[1][i+1][j-1]) addmovetoqueue(1,i+1,j-1,1),ok=1;
      if(!m[1][i+1][j]) addmovetoqueue(1,i+1,j,1),ok=1;
      if(!m[1][i+2][j-1]) addmovetoqueue(1,i+2,j-1,1),ok=1;
      if(!m[1][i+2][j]) addmovetoqueue(1,i+2,j,1),ok=1;
    }
    if(j<y-2 && mn[i][j]=='o' && mn[i][j+1]=='o' && mn[i][j+2]=='o') {
      if(!m[0][i-1][j]) addmovetoqueue(0,i-1,j,1),ok=1;
      if(!m[0][i][j]) addmovetoqueue(0,i,j,1),ok=1;
      if(!m[0][i-1][j+1]) addmovetoqueue(0,i-1,j+1,1),ok=1;
      if(!m[0][i][j+1]) addmovetoqueue(0,i,j+1,1),ok=1;
      if(!m[0][i-1][j+2]) addmovetoqueue(0,i-1,j+2,1),ok=1;
      if(!m[0][i][j+2]) addmovetoqueue(0,i,j+2,1),ok=1;
    }
  }
  return ok;
}

/*  if there are two blacks in a row, draw 2 edges out from them, extended in
    the direction of adjacency */
static int level1twoblack() {
  int i,j,k,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]=='*') {
    if(i<x-1 && mn[i+1][j]=='*') {
      /*  horizontal */
      for(k=-2;k<3;k++) if(k && !m[0][i+k][j]) addmovetoqueue(0,i+k,j,1),ok=1;
    }
    if(j<y-1 && mn[i][j+1]=='*') {
      /*  vertical */
      for(k=-2;k<3;k++) if(k && !m[1][i][j+k]) addmovetoqueue(1,i,j+k,1),ok=1;
    }
  }
  return ok;
}

/*  if a edge through a white is extended on one side and it can only turn one
    way on the other side, let it turn */
/*  this also takes care of two whites in a row */
static int level1whiteturn() {
  int i,j,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]=='o') {
    /*  first, find direction */
    if(i && m[0][i-1][j] && m[0][i][j]) {
      if(i>1 && m[0][i-2][j] && numedges(i+1,j)<2) {
        /*  horizontal, must turn at right */
        if(!isedgeallowed(1,i+1,j-1)) addmovetoqueue(1,i+1,j,1),ok=1;
        else if(!isedgeallowed(1,i+1,j)) addmovetoqueue(1,i+1,j-1,1),ok=1;
      } else if(i<x-2 && m[0][i+1][j] && numedges(i-1,j)<2) {
        /*  horizontal, must turn at left */
        if(!isedgeallowed(1,i-1,j-1)) addmovetoqueue(1,i-1,j,1),ok=1;
        else if(!isedgeallowed(1,i-1,j)) addmovetoqueue(1,i-1,j-1,1),ok=1;
      }
    } else if(j && m[1][i][j-1] && m[1][i][j]) {
      if(j>1 && m[1][i][j-2] && numedges(i,j+1)<2) {
        /*  vertical, must turn at bottom */
        if(!isedgeallowed(0,i-1,j+1)) addmovetoqueue(0,i,j+1,1),ok=1;
        else if(!isedgeallowed(0,i,j+1)) addmovetoqueue(0,i-1,j+1,1),ok=1;
      } else if(j<y-2 && m[1][i][j+1] && numedges(i,j-1)<2) {
        /*  vertical, must turn at left */
        if(!isedgeallowed(0,i-1,j-1)) addmovetoqueue(0,i,j-1,1),ok=1;
        else if(!isedgeallowed(0,i,j-1)) addmovetoqueue(0,i-1,j-1,1),ok=1;
      }
    }
  }
  return ok;
}

/*  if it is impossible to extend an edge of length 2 from a black dot,
    then there must be an edge of length 2 in the opposite direction */
static int level1blackblockeddir() {
  int i,j,ok=0,d,d2,x2,y2,x3,y3,bak;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]=='*' && numedges(i,j)<2) {
    for(d=0;d<4;d++) {
      d2=ed[d],x2=i+ex[d],y2=j+ey[d];
      /*  lines away from border already processed */
      if(i+2*dx[d]<0 || j+2*dy[d]<0 || i+2*dx[d]>=x || j+2*dy[d]>=y) continue;
      /*  check if there is already a straight 2-line: if so, ignore */
      if(m[d2][x2][y2] && m[d2][x2+dx[d]][y2+dy[d]]) continue;
      /*  check if there is an orthogonal line 2 away */
      if(isstraight2dir(d2^1,i+2*dx[d],j+2*dy[d])) goto otherdir;
      /*  check if there is a corner at distance 2 not facing towards
          black dot */
      if(i+3*dx[d]>=0 && j+3*dy[d]>=0 && i+3*dx[d]<x && i+3*dy[d]<y && iscorner(i+2*dx[d],j+2*dy[d])) {
        x3=i+ex[d]+2*dx[d],y3=j+ey[d]+2*dy[d];
        if(m[d2][x3][y3]) goto otherdir;
      }
      /*  check if edge of length 1 causes a corner in next cell */
      bak=m[d2][x2][y2];
      m[d2][x2][y2]=1;
      if(iscorner(i+dx[d],j+dy[d])) { m[d2][x2][y2]=bak; goto otherdir; }
      m[d2][x2][y2]=bak;
      continue;
    otherdir:
      x2=i+ex[d^2],y2=j+ey[d^2];
      x3=x2+dx[d^2],y3=y2+dy[d^2];
      if(!m[d2][x2][y2]) addmovetoqueue(d2,x2,y2,1),ok=1;
      if(!m[d2][x3][y3]) addmovetoqueue(d2,x3,y3,1),ok=1;
    }
  }
  return ok;
}

/*  if a loose end can only grow to one place, let it */
static int level1onealternative() {
  int i,j,ok=0,d,d2,x2,y2,d3=0,x3=0,y3=0,count;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(numedges(i,j)==1) {
    for(count=d=0,d3=x3=y3=-1;d<4;d++) {
      d2=ed[d];
      x2=i+ex[d];
      y2=j+ey[d];
      if(x2<0 || y2<0) continue;
      if(!d2 && x2==x-1) continue;
      if(d2 && y2==y-1) continue;
      if(m[d2][x2][y2]) continue;
      if(isedgeallowed(d2,x2,y2)) count++,d3=d2,x3=x2,y3=y2;
    }
    if(count==1) addmovetoqueue(d3,x3,y3,1),ok=1;
  }
  return ok;
}

/*  if a loose end is within a white, let it continue through in the same
    direction */
static int level1throughwhite() {
  int i,j,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]=='o') {
    if(i && m[0][i-1][j] && !m[0][i][j]) addmovetoqueue(0,i,j,1),ok=1;
    if(i && m[0][i][j] && !m[0][i-1][j]) addmovetoqueue(0,i-1,j,1),ok=1;
    if(j && m[1][i][j-1] && !m[1][i][j]) addmovetoqueue(1,i,j,1),ok=1;
    if(j && m[1][i][j] && !m[1][i][j-1]) addmovetoqueue(1,i,j-1,1),ok=1;
  }
  return ok;
}

/*  if an edge can go though a white dot in only one direction, then do it */
static int level1whiteonepossibility() {
  int i,j,ok=0;
  /*  process white dots without a 2-line through it */
  for(i=1;i<x-1;i++) for(j=1;j<y-1;j++) if(mn[i][j]=='o' && !isstraight2(i,j)) {
    /*  try vertical */
    /*  check for vertical line to left or right, or a corner next to it */
    if(isstraight2dir(1,i-1,j) || isstraight2dir(1,i+1,j) || iscorner(i-1,j) || iscorner(i+1,j)) {
      if(!m[1][i][j-1]) addmovetoqueue(1,i,j-1,1),ok=1;
      if(!m[1][i][j]) addmovetoqueue(1,i,j,1),ok=1;
    }
    /*  try horizontal */
    if(isstraight2dir(0,i,j-1) || isstraight2dir(0,i,j+1) || iscorner(i,j-1) || iscorner(i,j+1)) {
      if(!m[0][i-1][j]) addmovetoqueue(0,i-1,j,1),ok=1;
      if(!m[0][i][j]) addmovetoqueue(0,i,j,1),ok=1;
    }
  }
  return ok;
}

/*  if there is an edge of length 1 going out of a black dot, extend it */
static int level1extendblack() {
  int i,j,ok=0,d,d2,x2,y2;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]=='*') {
    for(d=0;d<4;d++) {
      d2=ed[d];
      x2=i+ex[d];
      y2=j+ey[d];
      if(x2<0 || y2<0) continue;
      if(d2 && y2==y-1) continue;
      if(!d2 && x2==x-1) continue;
      if(m[d2][x2][y2] && !m[d2][x2+dx[d]][y2+dy[d]])
        addmovetoqueue(d2,x2+dx[d],y2+dy[d],1),ok=1;
    }
  }
  return ok;
}

/*  line through a white dot such that it must turn in the next cell,
    and white 2 away or black 3 away.
    white: 2-line must be orthogonal
    black: 2-line in opposite direction, from black */
static int level1beyondmustturn() {
  int i,j,ok=0,k;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]=='o') {
    /*  must turn at right */
    if(i>1 && isstraight3dir(0,i-2,j)) {
      if(i<x-2 && mn[i+2][j]=='o') {
        if(!m[1][i+2][j-1]) addmovetoqueue(1,i+2,j-1,1),ok=1;
        if(!m[1][i+2][j]) addmovetoqueue(1,i+2,j,1),ok=1;
      }
      for(k=0;k<2;k++) if(i<x-5-k && mn[i+2+k][j]=='*') {
        if(!m[0][i+2+k][j]) addmovetoqueue(0,i+2+k,j,1),ok=1;
        if(!m[0][i+3+k][j]) addmovetoqueue(0,i+3+k,j,1),ok=1;
      }
    }
    /*  must turn at left */
    if(i && isstraight3dir(0,i-1,j)) {
      if(i>1 && mn[i-2][j]=='o') {
        if(!m[1][i-2][j-1]) addmovetoqueue(1,i-2,j-1,1),ok=1;
        if(!m[1][i-2][j]) addmovetoqueue(1,i-2,j,1),ok=1;
      }
      for(k=0;k<2;k++) if(i>3+k && mn[i-2-k][j]=='*') {
        if(!m[0][i-3-k][j]) addmovetoqueue(0,i-3-k,j,1),ok=1;
        if(!m[0][i-4-k][j]) addmovetoqueue(0,i-4-k,j,1),ok=1;
      }
    }
    /*  must turn at down */
    if(j>1 && isstraight3dir(1,i,j-2)) {
      if(j<y-2 && mn[i][j+2]=='o') {
        if(!m[0][i-1][j+2]) addmovetoqueue(0,i-1,j+2,1),ok=1;
        if(!m[0][i][j+2]) addmovetoqueue(0,i,j+2,1),ok=1;
      }
      for(k=0;k<2;k++) if(j<y-5-k && mn[i][j+2+k]=='*') {
        if(!m[1][i][j+2+k]) addmovetoqueue(1,i,j+2+k,1),ok=1;
        if(!m[1][i][j+3+k]) addmovetoqueue(1,i,j+3+k,1),ok=1;
      }
    }
    /*  must turn at up */
    if(j && isstraight3dir(1,i,j-1)) {
      if(j>1 && mn[i][j-2]=='o') {
        if(!m[0][i-1][j-2]) addmovetoqueue(0,i-1,j-2,1),ok=1;
        if(!m[0][i][j-2]) addmovetoqueue(0,i,j-2,1),ok=1;
      }
      for(k=0;k<2;k++) if(j>3+k && mn[i][j-2-k]=='*') {
        if(!m[1][i][j-3-k]) addmovetoqueue(1,i,j-3-k,1),ok=1;
        if(!m[1][i][j-4-k]) addmovetoqueue(1,i,j-4-k,1),ok=1;
      }
    }

  }
  return ok;
}

/*  TODO two black dots of distance 2 away: if one has an edge pointing away,
    the other must also have it */
/*  see C069, lower left for example */
static int level1black2pointaway() {
  int i,j,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]=='*') {
    /*  horizontal */
    if(i>1 && i<x-4 && mn[i+2][j]=='*') {
      if(isstraight2dir(0,i-1,j)) {
        if(!m[0][i+2][j]) addmovetoqueue(0,i+2,j,1),ok=1;
        if(!m[0][i+3][j]) addmovetoqueue(0,i+3,j,1),ok=1;
      } else if(isstraight2dir(0,i+3,j)) {
        if(!m[0][i-2][j]) addmovetoqueue(0,i-2,j,1),ok=1;
        if(!m[0][i-1][j]) addmovetoqueue(0,i-1,j,1),ok=1;
      }
    }
    /*  vertical */
    if(j>1 && j<y-4 && mn[i][j+2]=='*') {
      if(isstraight2dir(1,i,j-1)) {
        if(!m[1][i][j+2]) addmovetoqueue(1,i,j+2,1),ok=1;
        if(!m[1][i][j+3]) addmovetoqueue(1,i,j+3,1),ok=1;
      } else if(isstraight2dir(1,i,j+3)) {
        if(!m[1][i][j-2]) addmovetoqueue(1,i,j-2,1),ok=1;
        if(!m[1][i][j-1]) addmovetoqueue(1,i,j-1,1),ok=1;
      }
    }
  }
  return ok;
}

/*  for the pattern *.oo, black must have edge away from oo */
static int level1blacknear2white() {
  int i,j,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]=='*' && numedges(i,j)<2) {
    /*  check for illegal right */
    if(i<x-3 && mn[i+2][j]=='o' && mn[i+3][j]=='o') {
      if(!m[0][i-1][j]) addmovetoqueue(0,i-1,j,1),ok=1;
      if(!m[0][i-2][j]) addmovetoqueue(0,i-2,j,1),ok=1;
    }
    /*  check left */
    if(i>2 && mn[i-2][j]=='o' && mn[i-3][j]=='o') {
      if(!m[0][i][j]) addmovetoqueue(0,i,j,1),ok=1;
      if(!m[0][i+1][j]) addmovetoqueue(0,i+1,j,1),ok=1;
    }
    /*  check down */
    if(j<y-3 && mn[i][j+2]=='o' && mn[i][j+3]=='o') {
      if(!m[1][i][j-1]) addmovetoqueue(1,i,j-1,1),ok=1;
      if(!m[1][i][j-2]) addmovetoqueue(1,i,j-2,1),ok=1;
    }
    /*  check up */
    if(j>2 && mn[i][j-2]=='o' && mn[i][j-3]=='o') {
      if(!m[1][i][j]) addmovetoqueue(1,i,j,1),ok=1;
      if(!m[1][i][j+1]) addmovetoqueue(1,i,j+1,1),ok=1;
    }
  }
  return ok;
}

static int level1hint() {
  if(level1blacknearborder()) return 1;
  if(level1whitenearborder()) return 1;
  if(level1threewhite()) return 1;
  if(level1twoblack()) return 1;
  if(level1whiteturn()) return 1;
  if(level1extendblack()) return 1;
  if(level1onealternative()) return 1;
  if(level1throughwhite()) return 1;
  if(level1whiteonepossibility()) return 1;
  if(level1blackblockeddir()) return 1;
  if(level1beyondmustturn()) return 1;
  if(level1black2pointaway()) return 1;
  if(level1blacknear2white()) return 1;
  return 0;
}

static int level2hint() {
  return 0;
}

/*  if there are two possible moves from a loose end and one of them causes a
    closed loop which doesn't include all dots and edges, then the other
    move must be taken */
static int level3avoidloop() {
  int i,j,count,d,d2,x2,y2,dirs[4],k,l;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(numedges(i,j)==1) {
    /*  count eligible directions */
    for(d=count=0;d<4;d++) {
      d2=ed[d],x2=i+ex[d],y2=j+ey[d];
      if(m[d2][x2][y2] || !isedgeallowed(d2,x2,y2)) continue;
      dirs[count++]=d;
    }
    if(count!=2) continue;
    /*  try eligible directions */
    for(d=0;d<2;d++) {
      d2=ed[dirs[d]],x2=i+ex[dirs[d]],y2=j+ey[dirs[d]];
      m[d2][x2][y2]=1;
      followedge(i,j);
      /* check if:
         all cells in loop has numedges==2 AND 
         (there are nonvisited edges outside loop OR there are white/black
          dots with numedges<2) */
      for(k=0;k<qe;k+=2) if(numedges(q[k],q[k+1])==1) goto noloop;
      for(k=0;k<x;k++) for(l=0;l<y;l++) {
        if(mn[k][l]!='.' && numedges(k,l)<2) goto yesloop;
        if(numedges(k,l) && !visit[k][l]) goto yesloop;
      }
      /*  it seems we have a winning move */
      m[d2][x2][y2]=0;
      cleanupbfs();
      addmovetoqueue(d2,x2,y2,1);
      return 1;
    yesloop:
      /*  found non-solving loop! choose other alternative */
      m[d2][x2][y2]=0;
      cleanupbfs();
      d2=ed[dirs[d^1]],x2=i+ex[dirs[d^1]],y2=j+ey[dirs[d^1]];
      addmovetoqueue(d2,x2,y2,1);
      return 1;
    noloop:
      m[d2][x2][y2]=0;
      cleanupbfs();
    }
  }
  return 0;
}

/*  check if a move causes an odd number of loose ends in a region */
/*  check bottom of S03 */
static int level3oddloose() {
  int i,j,d2,x2,y2,count,d,dirs[4],tot,dummy;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(numedges(i,j)==1) {
    /*  first, check if there are only two allowed moves */
    for(count=d=0;d<4;d++) {
      d2=ed[d],x2=i+ex[d],y2=j+ey[d];
      if(x2<0 || y2<0) continue;
      if(d2 && y2==y-1) continue;
      if(!d2 && x2==x-1) continue;
      if(m[d2][x2][y2] || !isedgeallowed(d2,x2,y2)) continue;
      dirs[count++]=d;
    }
    if(count!=2) continue;
    for(d=0;d<2;d++) {
      /*  apply edge, check for odd region */
      d2=ed[dirs[d]],x2=i+ex[dirs[d]],y2=j+ey[dirs[d]];
      m[d2][x2][y2]=1;
      genericbfs(i+dx[d],j+dy[d],0,&tot,&dummy);
      cleanupbfs();
      m[d2][x2][y2]=0;
      if(tot&1) {
        d2=ed[dirs[d^1]],x2=i+ex[dirs[d^1]],y2=j+ey[dirs[d^1]];
        addmovetoqueue(d2,x2,y2,1);
        return 1;
      }
    }
  }
  return 0;
}

static int level3hint() {
  if(level3avoidloop()) return 1;
  if(level3oddloose()) return 1;
  return 0;
}

static int level4hint() {
  return 0;
}

#define NOTHING -50
#define NOINTERSECT -100
static int lev5res[2][MAXS][MAXS];

static int dogreedy(int lev) {
  int r;
  while(1) {
    r=verifyboard();
    if(r<0) return -1;
    if(r>0) return 0;
    if(level1hint()) goto theend;
    if(lev>1 && level2hint()) goto theend;
    if(lev>2 && level3hint()) goto theend;
    if(lev>3 && level4hint()) goto theend;
    break;
  theend:
    silentexecutemovequeue();
  }
  return 0;
}

/*  only consider edge extensions of loose ends */
static int level5intersection(int lev) {
  static int i=0,j=0;
  int z=x*y,oldsp=getstackpos(),k,l,d,count,d2,x2,y2,h,r=0;
  while(z--) {
    if(numedges(i,j)!=1) goto increase;
    for(count=d=0;d<4;d++) {
      d2=ed[d],x2=i+ex[d],y2=j+ey[d];
      if(m[d2][x2][y2]) continue;
      if(isedgeallowed(d2,x2,y2)) count++;
    }
    if(count<2) goto increase;
    /*  assume each direction in turn */
    for(k=0;k<x;k++) for(l=0;l<y;l++) lev5res[0][k][l]=lev5res[1][k][l]=NOTHING;
    for(d=0;d<4;d++) {
      d2=ed[d],x2=i+ex[d],y2=j+ey[d];
      if(m[d2][x2][y2] || !isedgeallowed(d2,x2,y2)) continue;
      domove(d2,x2,y2,1);
      r=dogreedy(lev);
      if(r>0) {
        while(getstackpos()>oldsp) undo(0);
        addmovetoqueue(d2,x2,y2,1);
        return 1;
      } else if(r<0) goto fail;
      for(h=0;h<2;h++) for(k=0;k<x;k++) for(l=0;l<y;l++) {
        if(lev5res[h][k][l]==NOTHING) lev5res[h][k][l]=m[h][k][l];
        else if(lev5res[h][k][l]!=m[h][k][l]) lev5res[h][k][l]=NOINTERSECT;
      }
    fail:
      while(getstackpos()>oldsp) undo(0);
    }
    for(r=h=0;h<2;h++) for(k=0;k<x;k++) for(l=0;l<y;l++) if(lev5res[h][k][l]==1 && lev5res[h][k][l] && !m[h][k][l]) {
      addmovetoqueue(h,k,l,1); r=1;
    }
    if(r) return 1;
  increase:
    j++;
    if(j==y) {
      j=0;
      i++;
      if(i==x) i=0,d^=1;
    }
  }
  return 0;
}

/*  try both ways of making a straight line through o */
static int level5tryall2linewhite(int lev) {
  static int i=0,j=0;
  int z=x*y,oldsp=getstackpos(),k,l,d,d2,x2,y2,h,r=0;
  while(z--) {
    /*  find o with no edges adjacent */
    if(mn[i][j]!='o' || numedges(i,j)) goto increase;
    for(k=0;k<x;k++) for(l=0;l<y;l++) lev5res[0][k][l]=lev5res[1][k][l]=NOTHING;
    for(d=0;d<2;d++) {
      d2=ed[d];
      x2=i-(d2==0);
      y2=j-(d2==1);
      domove(d2,i,j,1);
      domove(d2,x2,y2,1);
      r=dogreedy(lev);
      if(r>0) {
        while(getstackpos()>oldsp) undo(0);
        addmovetoqueue(d2,x2,y2,1);
        addmovetoqueue(d2,i,j,1);
        return 1;
      } else if(r<0) {
        while(getstackpos()>oldsp) undo(0);
        d2=ed[d^1];
        x2=i-(d2==0);
        y2=j-(d2==1);
        addmovetoqueue(d2,x2,y2,1);
        addmovetoqueue(d2,i,j,1);
        return 1;
      }
      for(h=0;h<2;h++) for(k=0;k<x;k++) for(l=0;l<y;l++) {
        if(lev5res[h][k][l]==NOTHING) lev5res[h][k][l]=m[h][k][l];
        else if(lev5res[h][k][l]!=m[h][k][l]) lev5res[h][k][l]=NOINTERSECT;
      }
      while(getstackpos()>oldsp) undo(0);
    }
    for(r=h=0;h<2;h++) for(k=0;k<x;k++) for(l=0;l<y;l++) if(lev5res[h][k][l]==1 && lev5res[h][k][l] && !m[h][k][l]) {
      addmovetoqueue(h,k,l,1); r=1;
    }
    if(r) return 1;
  increase:
    j++;
    if(j==y) {
      j=0;
      i++;
      if(i==x) i=0;
    }
  }
  return 0;
}

/*  try all 4 ways of fulfilling black */
static int level5tryallblack(int lev) {
  static int i=0,j=0;
  int z=x*y,oldsp=getstackpos(),k,l,d,dd[4],xx[4],yy[4],e,r,h;
  while(z--) {
    /*  find incomplete * */
    if(mn[i][j]!='*' || numedges(i,j)>1) goto increase;
    for(k=0;k<x;k++) for(l=0;l<y;l++) lev5res[0][k][l]=lev5res[1][k][l]=NOTHING;
    for(d=0;d<4;d++) {
      e=(d+1)&3;
      dd[0]=dd[1]=ed[d]; dd[2]=dd[3]=ed[e];
      xx[0]=i+ex[d]; yy[0]=j+ey[d]; xx[1]=xx[0]+dx[d]; yy[1]=yy[0]+dy[d];
      xx[2]=i+ex[e]; yy[2]=j+ey[e]; xx[3]=xx[2]+dx[e]; yy[3]=yy[2]+dy[e];
      for(k=0;k<4;k++) {
        if(!m[dd[k]][xx[k]][yy[k]]) domove(dd[k],xx[k],yy[k],1);
      }
      if(verifyboard()<0) {
        while(getstackpos()>oldsp) undo(0);
        goto increase;
      }
      r=dogreedy(lev);
      if(r>0) {
        while(getstackpos()>oldsp) undo(0);
        for(k=0;k<4;k++) if(!m[dd[k]][xx[k]][yy[k]]) domove(dd[k],xx[k],yy[k],1);
        return 1;
      } else if(r==0) {
        for(h=0;h<2;h++) for(k=0;k<x;k++) for(l=0;l<y;l++) {
          if(lev5res[h][k][l]==NOTHING) lev5res[h][k][l]=m[h][k][l];
          else if(lev5res[h][k][l]!=m[h][k][l]) lev5res[h][k][l]=NOINTERSECT;
        }
      }
      while(getstackpos()>oldsp) undo(0);
    }
    for(r=h=0;h<2;h++) for(k=0;k<x;k++) for(l=0;l<y;l++) if(lev5res[h][k][l]==1 && lev5res[h][k][l] && !m[h][k][l]) {
      addmovetoqueue(h,k,l,1); r=1;
    }
    if(r) return 1;
  increase:
    j++;
    if(j==y) {
      j=0;
      i++;
      if(i==x) i=0;
    }
  }
  return 0;
}

/*  todo intersection:
    - try all 4 ways of fulfilling a black
    - try all 8 ways of fulfilling a white (including turn on each side)  */
static int level5hint() {
  if(level5intersection(4)) return 1;
  if(level5tryall2linewhite(4)) return 1;
  if(level5tryallblack(4)) return 1;
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
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && numedges(i,j)==1) {
    qs2=qs;
    followedge(i,j);
    for(k=qs2;k<qe;k+=2) updateedge(q[k],q[k+1],colarray[col]);
    col=(col+1)%COLSIZE;
  }
  /*  then, search through all unvisited closed loops */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && numedges(i,j)==2) {
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
  } else if(key==SDLK_j) {  /*  temporary: superhintkey */
    res=hint();
    if(res>0) {
      executemovequeue();
      while(hint()>0) executemovequeue();
      if(verifyboard()<1) messagebox("Sorry, no more moves found.");
    } else if(!res) messagebox("Sorry, no moves found.");
    else messagebox("Sorry, hint will not work on an illegal board.");
  }
}

static int togglecell(int val) {
  return val^1;
}

static void processmousemotion() {
  int prevcellx,prevcelly;
  int cellx,celly,t,celld;
  if(mousebuttons[0]) {
    getcell(event_mousefromx,event_mousefromy,&prevcellx,&prevcelly);
    getcell(event_mousex,event_mousey,&cellx,&celly);
    if(cellx<0 || celly<0 || cellx>=x || celly>=y) return;
    if(prevcellx<0 || prevcelly<0 || prevcellx>=x || prevcelly>=y) return;
    if(manhattandist(cellx,celly,prevcellx,prevcelly)==1) {
      if(prevcellx>cellx) t=cellx,cellx=prevcellx,prevcellx=t;
      if(prevcelly>celly) t=celly,celly=prevcelly,prevcelly=t;
      if(prevcellx==cellx) celld=1;
      else celld=0;
      domove(celld,prevcellx,prevcelly,togglecell(m[celld][prevcellx][prevcelly]));
      updatetoscreen(celld,prevcellx,prevcelly,1);
    }
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

void masyu(char *path,int solve) {
  int event;
  loadpuzzle(path);
  initbfs();
  drawgrid();
  if(solve) { autosolver(path); return; }
  do {
    event=getevent();
    switch(event) {
    case EVENT_RESIZE:
      drawgrid();
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
