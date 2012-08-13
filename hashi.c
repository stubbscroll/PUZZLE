#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

/*  the puzzle numbers (never changed after loading puzzle) */
/*  0:no node, 1-8: node with number */
static int mn[MAXS][MAXS];
/*  for each cell: cache the neighbouring cell in each direction */
/*  the value stored is the coordinate (only the index that changes, or -1 if
    edge or i,j is no node */
static int mcache[MAXS][MAXS][4];
/*  indicate edges: 0-2 means 0-2 edges */
/*  last dimension represent direction, same as dx[],dy[] */
/*  will only be set on nodes */
static int m[MAXS][MAXS][4];
/*  lower 4 bits regard edges:
    1: cell contains one horizontal edge, 2: cell contains 2 horizontal edges,
    4: cell contains one vertical edge, 8: cell contains 2 vertical edges */
static int st[MAXS][MAXS];

static char touched[MAXS][MAXS];  /*  1 if cell is changed and need to be redrawn */

/*  move queue for hint system */
/*  format: dir fromx fromy tox toy value */
#define MAXMQ MAXS*MAXS*6
static int mq[MAXMQ];
static int mqs,mqe;

int parsechar(char c) { return c=='.'?0:c-48; }

static void loadpuzzle(char *path) {
  static char s[MAXSTR];
  FILE *f=fopen(path,"r");
  int z=0,ln=0,i,j,px,py,k;
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
      for(i=j=0;j<x;j++) mn[j][ln]=parsechar(s[i++]);
      ln++;
    }
  }
  fclose(f);
  startx=10,starty=30;
  mqs=mqe=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    st[i][j]=touched[i][j]=0;
    m[i][j][0]=m[i][j][1]=m[i][j][2]=m[i][j][3]=0;
    mcache[i][j][0]=mcache[i][j][1]=mcache[i][j][2]=mcache[i][j][3]=-1;
  }
  /*  cache next cell for each direction */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]) {
    for(k=0;k<4;k++) {
      px=i+dx[k],py=j+dy[k];
      while(1) {
        if(px<0 || py<0 || px>=x || py>=y) {
          mcache[i][j][k]=-1;
          break;
        }
        if(mn[px][py]) {
          if(dx[k]) mcache[i][j][k]=px;
          else mcache[i][j][k]=py;
          break;
        }
        px+=dx[k]; py+=dy[k];
      }
    }
  }
  /*  calculate neighbours */
  for(i=0;i<x;i++) for(j=0;j<y;j++) for(k=0;k<4;k++) {
    px=i+dx[k],py=j+dy[k];
    while(px>=0 && py>=0 && px<x && py<y) {
      if(mn[px][py]) {
        if(dx[k]) mcache[i][j][k]=px;
        else mcache[i][j][k]=py;
        break;
      }
      px+=dx[k]; py+=dy[k];
    }
  }
}

/*  return 1 if we can move along this edge */
/*  assume that the coordinates are sane (has been run through findendpoints)
    and that they define a horizontal or vertical edge) */
static int canmove(int x1,int y1,int x2,int y2) {
  int celld=x1==x2,i,t;
  if(x1<0 || y1<0 || x2<0 || y2<0) return 0;
  /*  check for crossing orthogonal edge */
  if(celld) {
    if(y1>y2) t=y1,y1=y2,y2=t;
    for(i=y1;i<y2;i++) if(st[x1][i]&3) return 0;
  } else {
    if(x1>x2) t=x1,x1=x2,x2=t;
    for(i=x1;i<x2;i++) if(st[i][y1]&12) return 0;
  }
  return 1;
}

/*  return number of edges connected to u,v */
static int numedges(int u,int v) {
  if(!mn[u][v]) return 0;
  return m[u][v][0]+m[u][v][1]+m[u][v][2]+m[u][v][3];
}

/*  determine the number of edges it is possible to add to (u,v) in direction d */
static int howmanycanbeadded(int u,int v,int d) {
  int p=u,q=v,left,cap;
  if(m[u][v][d]==2) return 0;
  if(d&1) q=mcache[u][v][d];
  else p=mcache[u][v][d];
  if(!canmove(p,q,u,v)) return 0;
  left=2-m[u][v][d];
  cap=mn[p][q]-m[p][q][0]-m[p][q][1]-m[p][q][2]-m[p][q][3];
  return left<cap?left:cap;
}

static int getneighbour(int u,int v,int d,int *x1,int *y1) {
  if(d&1) *y1=mcache[u][v][d],*x1=u;
  else *x1=mcache[u][v][d],*y1=v;
  return *x1>-1 && *y1>-1;
}

/*  BFS stuff */
static uchar visit[MAXS][MAXS];
static int qs,qe,q[MAXS*MAXS*2];

static void initbfs() {
  qs=qe=0;
  memset(visit,0,sizeof(visit));
}

/*  type 0: search along edges
    return: tot:  number of cells found
            done: number of cells having all edges */
static void genericbfs(int sx,int sy,int type,int *tot,int *done) {
  int cx,cy,x2,y2,d;
  *tot=1;
  *done=mn[sx][sy]==numedges(sx,sy);
  if(visit[sx][sy]) return;
  visit[sx][sy]=1;
  q[qe++]=sx; q[qe++]=sy;
  while(qs<qe) {
    cx=q[qs++]; cy=q[qs++];
    for(d=0;d<4;d++) if(m[cx][cy][d]) {
      x2=cx; y2=cy;
      if(d&1) y2=mcache[cx][cy][d];
      else x2=mcache[cx][cy][d];
      if(visit[x2][y2]) continue;
      (*tot)++;
      if(numedges(x2,y2)==mn[x2][y2]) (*done)++;
      q[qe++]=x2; q[qe++]=y2;
      visit[x2][y2]=1;
    }
  }
}

static void cleanupbfs() {
  while(qe) visit[q[qe-2]][q[qe-1]]=0,qe-=2;
  qs=0;
}

/*  return -1 if illegal (unsolvable) board, 0 if unfinished, 1 if solved */
static int verifyboard() {
  int i,j,tot,done,comp=0,closedloop=0;
  /*  look for nodes where sum edges > number */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j] && mn[i][j]<numedges(i,j)) return -1;
  /*  bfs through nodes and check number of components and unfilled cells */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j] && !visit[i][j]) {
    comp++;
    genericbfs(i,j,0,&tot,&done);
    if(tot==done) closedloop=1;
  }
  cleanupbfs();
  if(closedloop && comp<2) return 1;
  if(closedloop) return -1;
  return 0;
}

static int nodecolour(int u,int v) {
  int num=m[u][v][0]+m[u][v][1]+m[u][v][2]+m[u][v][3],canadd,dirs;
  int d1,d2,d3,d4;
  if(num==mn[u][v]) return okcol;
  else if(num>mn[u][v]) return errorcol;
  else {
    /*  yellow if mn[u][v]-num == sum free of all neighbours */
    d1=howmanycanbeadded(u,v,0);
    d2=howmanycanbeadded(u,v,1);
    d3=howmanycanbeadded(u,v,2);
    d4=howmanycanbeadded(u,v,3);
    dirs=(d1>0)+(d2>0)+(d3>0)+(d4>0);
    canadd=d1+d2+d3+d4;
    if(canadd+num==mn[u][v] || dirs==1) return almostokcol;
  }
  return blankcol;
}

/*  col=0:  use default colour, col=1: use specified colour */
static void updatecell(int u,int v,int col,Uint32 ourfilled,Uint32 ouredge) {
  int z,yy=font->height,tx,ty;
  int x1=(width-edgethick)/2,x2=(width-2*edgethick2-2)/2,x3=(width-3)/2;
  int y1=(height-edgethick)/2,y2=(height-2*edgethick2-2)/2,y3=(height-3)/2;
  double ww=width-3<width*0.8?width-3:width*0.8,w=width<height?width*.48:height*.48;
  char s[2]={0,0};
  if(x1<1) x1=1; if(y1<1) y1=1;
  if(x2<1) x2=1; if(y2<1) y2=1;
  if(ww<0) ww=w*0.7; else ww*=.5;
  if(mn[u][v]) {
    /*  numbered cell */
    drawhollowdisc(u,v,w,ww,col?ourfilled:filledcol,col?ouredge:nodecolour(u,v),blankcol);
    s[0]=mn[u][v]+48;
    /*  draw number in centre */
    z=sdl_font_width(font,"%s",s);
    tx=startx+u*width+((width-z)>>1)+1;
    ty=starty+v*height+((height-yy)>>1)+1;
    sdl_font_printf(screen,font,tx,ty,col?ourfilled:filledcol,col?ourfilled:filledcol,"%s",s);
  } else {
    /*  draw 1-2 horizontal, 1-2 vertical */
    drawrectangle32(startx+u*width,starty+v*height,startx+(u+1)*width,starty+(v+1)*height,blankcol);
    if(st[u][v]==1) drawrectangle32(startx+u*width,starty+v*height+y1,startx+(u+1)*width,starty+(v+1)*height-y1-1,col?ouredge:edgecol);
    else if(st[u][v]==2) {
      drawrectangle32(startx+u*width,starty+v*height+y2,startx+(u+1)*width,starty+v*height+y3,col?ouredge:edgecol);
      drawrectangle32(startx+u*width,starty+(v+1)*height-y3,startx+(u+1)*width,starty+(v+1)*height-y2,col?ouredge:edgecol);
    } else if(st[u][v]==4) drawrectangle32(startx+u*width+x1,starty+v*height,startx+(u+1)*width-x1-1,starty+(v+1)*height,col?ouredge:edgecol);
    else if(st[u][v]==8) {
      drawrectangle32(startx+u*width+x2,starty+v*height,startx+u*width+x3,starty+(v+1)*height,col?ouredge:edgecol);
      drawrectangle32(startx+(u+1)*width-x3,starty+v*height,startx+(u+1)*width-x2,starty+(v+1)*height,col?ouredge:edgecol);
    }
  }
}

static void partialredraw() {
  int i,j;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(touched[i][j]) updatecell(i,j,0,0,0);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(touched[i][j]) {
    sdlrefreshcell(i,j);
    touched[i][j]=0;
  }
}

static void updatetoscreen(int visible) {
  if(visible) partialredraw();
}

static void drawgrid() {
  int i,j;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  clear32(WHITE32);
  updatescale(resx-startx,resy-starty,x,y,thick);
  for(i=0;i<x;i++) for(j=0;j<y;j++) updatecell(i,j,0,0,0);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
}

/*  find endpoints (numbered nodes) for the given partial edge */
/*  WARNING, this routine depends on the order of the contents in dx[],dy[]
    which is right(+x), down(+y), left(-x), up(-y) */
static void findendpoints(int *x1,int *y1,int *x2,int *y2) {
  int celld=*x1==*x2;
  if(!mn[*x1][*y1]) {
    if(celld) *y1=mcache[*x1][*y1][3];
    else *x1=mcache[*x1][*y1][2];
  }
  if(!mn[*x2][*y2]) {
    if(celld) *y2=mcache[*x2][*y2][1];
    else *x2=mcache[*x2][*y2][0];
  }
}

/*  "draw" an edge in data structures, touch them to force redraw */
/*  NB, somewhat convoluted logic to propagate correct colours to nearby cells */
static void drawline(int d,int x1,int y1,int x2,int y2,int old,int val) {
  int i,t,z=!old || !val;
  if(x1>x2) t=x1,x1=x2,x2=t;
  if(y1>y2) t=y1,y1=y2,y2=t;
  touched[x1][y1]=touched[x2][y2]=1;
  for(i=0;i<4;i++) if(mcache[x1][y1][i]>-1) touched[i&1?x1:mcache[x1][y1][i]][i&1?mcache[x1][y1][i]:y1]=1;
  for(i=0;i<4;i++) if(mcache[x2][y2][i]>-1) touched[i&1?x2:mcache[x2][y2][i]][i&1?mcache[x2][y2][i]:y2]=1;
  if(d&1) {
    for(i=y1+1;i<y2;i++) st[x1][i]=val<<2,touched[x1][i]=1;
    if(z) for(i=y1+1;i<y2;i++) {
      t=mcache[x1][i][0]; if(t>-1) touched[t][i]=1;
      t=mcache[x1][i][2]; if(t>-1) touched[t][i]=1;
    }
  } else {
    for(i=x1+1;i<x2;i++) st[i][y1]=val,touched[i][y1]=1;
    if(z) for(i=x1+1;i<x2;i++) {
      t=mcache[i][y1][1]; if(t>-1) touched[i][t]=1;
      t=mcache[i][y1][3]; if(t>-1) touched[i][t]=1;
    }
  }
}

static void domove(int d,int x1,int y1,int x2,int y2,int val) {
  int old=m[x1][y1][d];
  if(val==m[x1][y1][d]) error("logical error, tried to set to existing value");
  stackpush(d); stackpush(x1); stackpush(y1); stackpush(x2);
  stackpush(y2); stackpush(old);
  m[x1][y1][d]=val;
  m[x2][y2][d^2]=val;
  drawline(d,x1,y1,x2,y2,old,val);
}

static void domovedir(int d,int x1,int y1,int val) {
  int x2,y2;
  getneighbour(x1,y1,d,&x2,&y2);
  domove(d,x1,y1,x2,y2,val);
}

static void undo(int visible) {
  int old;
	int val=stackpop(),y2=stackpop(),x2=stackpop(),y1=stackpop(),x1=stackpop(),d=stackpop();
  if(!stackempty()) {
    old=m[x1][y1][d];
    m[x1][y1][d]=val;
    m[x2][y2][d^2]=val;
    drawline(d,x1,y1,x2,y2,old,val);
    updatetoscreen(visible);
  }
}

/*  start of hint system! */

static void addmovetoqueue(int d,int x1,int y1,int x2,int y2,int val) {
  mq[mqe++]=d; mq[mqe++]=x1; mq[mqe++]=y1; mq[mqe++]=x2; mq[mqe++]=y2; mq[mqe++]=val;
  if(mqe==MAXMQ) mqe=0;
}

static void adddirtoqueue(int d,int x1,int y1,int val) {
  int x2,y2;
  getneighbour(x1,y1,d,&x2,&y2);
  addmovetoqueue(d,x1,y1,x2,y2,val);
}

static int movequeueisempty() {
  return mqs==mqe;
}

/*  return 0:no moves in queue, 1:move successfully executed */
static int executeonemovefromqueue(int visible) {
loop:
  if(movequeueisempty()) return 0;
  /*  the hint system can produce some moves twice, don't redo moves */
  if(m[mq[mqs+1]][mq[mqs+2]][mq[mqs]]==mq[mqs+5]) {
    mqs+=6;
    if(mqs==MAXMQ) mqs=0;
    goto loop;
  }
  domove(mq[mqs],mq[mqs+1],mq[mqs+2],mq[mqs+3],mq[mqs+4],mq[mqs+5]);
  updatetoscreen(visible);
  mqs+=6;
  if(mqs==MAXMQ) mqs=0;
  return 1;
}

static void executemovequeue() {
  while(executeonemovefromqueue(1)) if(dummyevent()) drawgrid();
}

static void silentexecutemovequeue() {
  while(executeonemovefromqueue(0));
}

/*  remaining possible edges for node equals number of edges possible to add */
static int level1maximalclue() {
  int i,j,num,has,a[4],k,x1,y1;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if((num=mn[i][j])) {
    has=numedges(i,j);
    if(num==has) continue;
    for(k=0;k<4;k++) {
      a[k]=howmanycanbeadded(i,j,k);
      if(a[k]>2-m[i][j][k]) a[k]=2-m[i][j][k];
      if(a[k]>num-has) a[k]=num-has;
    }
    if(a[0]+a[1]+a[2]+a[3]+has==num) {
      for(k=0;k<4;k++) if(a[k]) {
        getneighbour(i,j,k,&x1,&y1);
        addmovetoqueue(k,i,j,x1,y1,a[k]+m[i][j][k]);
      }
      return 1;
    }
  }
  return 0;
}

/*  n available edges, >=2n-1 number, so at least one edge in each direction */
static int level1mustaddall() {
  int i,j,d,e[4],k,x1,y1,ok=0,num;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j] && numedges(i,j)<mn[i][j]) {
    num=mn[i][j];
    for(d=k=0;k<4;k++) {
      d+=e[k]=howmanycanbeadded(i,j,k)>0 || m[i][j][k]==1;
      if(m[i][j][k]==2) num-=2;
    }
    if(num>=2*d-1) {
      for(k=0;k<4;k++) if(e[k] && !m[i][j][k]) {
        getneighbour(i,j,k,&x1,&y1);
        addmovetoqueue(k,i,j,x1,y1,1);
        ok=1;
      }
      if(ok) return 1;
    }
  }
  return 0;
}

static int level1hint() {
  if(level1maximalclue()) return 1;
  if(level1mustaddall()) return 1;
  return 0;
}

/*  a 2 with 2 ways: if one of alternatives is a 1 or 2, take the other edge */
static int level2corner2() {
  int i,j,k,b[4],c,d,x1,y1,e;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]==2) {
    for(c=k=0;k<4;k++) if(howmanycanbeadded(i,j,k) || m[i][j][k]) b[c++]=k;
    if(c==2) {
      for(k=0;k<2;k++) {
        d=b[k]; e=b[k^1];
        getneighbour(i,j,d,&x1,&y1);
        if(mn[x1][y1]<3 && !m[i][j][e]) {
          getneighbour(i,j,e,&x1,&y1);
          addmovetoqueue(e,i,j,x1,y1,1);
          return 1;
        }
      }
    }
  }
  return 0;
}

/*  a 1 with >=2 ways: if all but one alternative is 1, take the other edge */
static int level2corner1() {
  int i,j,k,num1,num2,ix=-1,x1,y1;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]==1 && !numedges(i,j)) {
    for(num1=num2=k=0;k<4;k++) if(howmanycanbeadded(i,j,k)) {
      getneighbour(i,j,k,&x1,&y1);
      if(mn[x1][y1]>1) num2++,ix=k; else num1++;
    }
    if(num2!=1) continue;
    adddirtoqueue(ix,i,j,1);
    return 1;
  }
  return 0;
}

/* check the general case if a missing edge makes it impossible to fulfill the node */
static int level2mustedge() {
  int i,j,k,sum,num,a[4],can;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j] && (num=numedges(i,j))<mn[i][j]) {
    for(sum=k=0;k<4;k++) sum+=a[k]=howmanycanbeadded(i,j,k);
    for(k=0;k<4;k++) if(!m[i][j][k] && a[k]) {
      can=num+sum-a[k];
      if(can<mn[i][j]) {
        adddirtoqueue(k,i,j,1);
        return 1;
      }
    }
  }
  return 0;
}

static int level2hint() {
  if(level2corner2()) return 1;
  if(level2corner1()) return 1;
  if(level2mustedge()) return 1;
  return 0;
}

/*  if a node has one edge left, and each move except one causes an illegal position
    (more specifically: disconnected subgraphs) then do the only legal move */
static int level3disconnect() {
  int i,j,k,x1,y1,ways,dir=-1;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]==numedges(i,j)+1) {
    for(ways=k=0;k<4;k++) if(howmanycanbeadded(i,j,k)) {
      getneighbour(i,j,k,&x1,&y1);
      domove(k,i,j,x1,y1,m[i][j][k]+1);
      if(verifyboard()>-1) {
        ways++;
        dir=k;
      }
      undo(0);
      if(ways>1) break;
    }
    if(ways==1) {
      adddirtoqueue(dir,i,j,1);
      return 1;
    }
  }
  return 0;
}

static int level3hint() {
  if(level3disconnect()) return 1;
  return 0;
}

static int level4hint() {
  return 0;
}

static int lev5min[MAXS][MAXS][4];

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

/*  try all ways of satisfying a node, then brute and intersect */
static int level5brutemin(int lev) {
  static int i=0,j=0;
  int z=x*y,a[4],k,a1,a2,a3,a0,num,r,l,v,ok=0,oldsp=getstackpos();
	if(i>=x) i=0; if(j>=y) j=0;
  while(z--) {
    if(!mn[i][j] || mn[i][j]==numedges(i,j)) goto increase;
    num=numedges(i,j);
    for(k=0;k<4;k++) a[k]=howmanycanbeadded(i,j,k);
    for(k=0;k<x;k++) for(l=0;l<y;l++) if(mn[k][l]) for(v=0;v<4;v++) lev5min[k][l][v]=2;
    for(a0=0;a0<=a[0];a0++) for(a1=0;a1<=a[1];a1++) for(a2=0;a2<=a[2];a2++) {
      a3=mn[i][j]-num-a0-a1-a2;
      if(a3<0 || a3>a[3]) continue;
      if(a0) domovedir(0,i,j,m[i][j][0]+a0);
      if(a1) domovedir(1,i,j,m[i][j][1]+a1);
      if(a2) domovedir(2,i,j,m[i][j][2]+a2);
      if(a3) domovedir(3,i,j,m[i][j][3]+a3);
      updatetoscreen(0);
      r=dogreedy(lev);
      if(r>-1) for(k=0;k<x;k++) for(l=0;l<y;l++) if(mn[k][l]) for(v=0;v<4;v++) {
        if(lev5min[k][l][v]>m[k][l][v]) lev5min[k][l][v]=m[k][l][v];
      }
      while(getstackpos()>oldsp) undo(0);
		}
    /*  apply */
    for(k=0;k<x;k++) for(l=0;l<y;l++) if(mn[k][l]) for(v=0;v<4;v++) {
      if(lev5min[k][l][v]>m[k][l][v]) {
        ok=1;
        adddirtoqueue(v,k,l,lev5min[k][l][v]);
      }
    }
    if(ok) return 1;
  increase:
    i++; if(i==x) {
      i=0; j++; if(j==y) j=0;
    }
  }
  return 0;
}

static int level5hint() {
  if(level5brutemin(4)) return 1;
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

/*  edges and node exterior */
static Uint32 colarray[]={0x000055, 0x0000AA, 0x0000FF, 0x006D00, 0x0092AA, 0x920000, 0x496DAA};
/*  node interior, satisfied nodes */
static Uint32 colarray2[]={0x5555FF, 0xAAAAFF, 0xCCCCFF, 0x6FDD6F, 0xAACCFF, 0xFF9292, 0xC0E0FF};
/*  node interior, hungry nodes */
static Uint32 colarray3[]={0xeeeeFF, 0xf2f2FF, 0xf5f5FF, 0xf0f86F, 0xf2f8FF, 0xFFeded, 0xf0f8FF};
#define COLSIZE 7

static int forcefullredraw;

static void drawverify() {
  int i,j,k,col=0,qs2,tot,dummy,l,z;
  if(forcefullredraw) drawgrid();
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  /*  search through all nodes, put colour on nodes+edges */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && mn[i][j]) {
    qs2=qs;
    genericbfs(i,j,0,&tot,&dummy);
    /*  colour solo nodes with plain b/w */
    if(tot<2) {
      updatecell(i,j,1,filledcol,blankcol);
      continue;
    }
    for(k=qs2;k<qe;k+=2) {
      updatecell(q[k],q[k+1],1,colarray[col],mn[q[k]][q[k+1]]>numedges(q[k],q[k+1])?colarray3[col]:colarray2[col]);
      /*  draw edges */
      if(m[q[k]][q[k+1]][0]) {
        /*  draw right */
        z=mcache[q[k]][q[k+1]][0];
        for(l=q[k]+1;l<z;l++) updatecell(l,q[k+1],1,colarray[col],colarray2[col]);
      }
      if(m[q[k]][q[k+1]][1]) {
        /*  draw down */
        z=mcache[q[k]][q[k+1]][1];
        for(l=q[k+1]+1;l<z;l++) updatecell(q[k],l,1,colarray[col],colarray2[col]);
      }
    }
    col=(col+1)%COLSIZE;
  }
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
  cleanupbfs();
}
#undef COLSIZE

static void showverify() {
  int i,j;
  forcefullredraw=0;
  drawverify();
  forcefullredraw=1;
  anykeypress(drawverify);
  /*  display normal level */
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) updatecell(i,j,0,0,0);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
}

static void processkeydown(int key) {
  int res;
  if(key==undokey) undo(1);
  else if(key==verifykey) showverify();
  else if(key==hintkey) {
    if(!executeonemovefromqueue(1)) {
      res=hint();
      if(res>0) executeonemovefromqueue(1);
      else if(!res) messagebox(1,"Sorry, no moves found.");
      else messagebox(1,"Sorry, hint will not work on an illegal board.");
    }
  } else if(key==SDLK_j) {
    res=hint();
    if(res>0) {
      executemovequeue();
      while(hint()>0) executemovequeue();
      if(verifyboard()<1) messagebox(1,"Sorry, no more moves found.");
    } else if(!res) messagebox(1,"Sorry, no moves found.");
    else messagebox(1,"Sorry, hint will not work on an illegal board.");
	}
}

/*  set to 1 when a button is held and a move is done
    set to 0 when mouse button is released
    the purpose is to prevent multiple moves in one long "movement" */
static int mousestate;

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
      /*  find endpoints */
      findendpoints(&prevcellx,&prevcelly,&cellx,&celly);
      if(!mousestate && canmove(prevcellx,prevcelly,cellx,celly)) {
        /*  change an edge that passes through (prevcellx,prevcelly) and
            (cellx,celly). the edge is horizontal if celld=0, otherwise
            it's vertical (celld=1). it is allowed that one of these points
            are on a numbered cell (but in this case, don't follow the pointer */
        domove(celld,prevcellx,prevcelly,cellx,celly,(m[prevcellx][prevcelly][celld]+1)%3);
        updatetoscreen(1);
        mousestate=1;
      }
    }
  } else mousestate=0;
}

static void autosolver(char *s) {
  int res,i,j;
	char t[256];
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
	for(j=i=0;s[i];i++) if(s[i]=='/' || s[i]=='\\') j=i+1;
	for(i=0;s[j];) t[i++]=s[j++];
	for(i=0;t[i] && t[i]!='.';i++);
	if(!t[i]) t[i++]='.';
	else i++;
	t[i++]='b'; t[i++]='m'; t[i++]='p'; t[i]=0;
	SDL_SaveBMP(screen,t);
}

void hashiwokakero(char *path,int solve) {
  int event;
  loadpuzzle(path);
  mousestate=0;
  initbfs();
  drawgrid();
  if(solve) { autosolver(path); return; }
  do {
    event=getevent();
    switch(event) {
    case EVENT_RESIZE:
      drawgrid();
    case EVENT_NOEVENT:
      break;
    case EVENT_MOUSEMOTION:
      processmousemotion();
      if(verifyboard()>0) {
        messagebox(1,"You are winner!");
        return;
      }
      break;
    default:
      /*  catch intervals of values here */
      if(event>=EVENT_KEYDOWN && event<EVENT_KEYUP) {
        processkeydown(event-EVENT_KEYDOWN);
        if(verifyboard()>0) {
          messagebox(1,"You are winner!");
          return;
        }
      }
    }
  } while(event!=EVENT_QUIT && !keys[SDLK_ESCAPE]);
}
