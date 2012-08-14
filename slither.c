#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "SDL/SDL.h"
#include "puzzle.h"

/*  max map size */
#define MAXS 128
/*  max string length */
#define MAXSTR 1024
static int x,y;                 /*  map size */
static char difficulty[MAXSTR]; /*  string holding the difficulty */

/*  edges filled in by the player, divided into horizontal [0] and
    vertical [1] arrays */
#define UNFILLED -3
#define EMPTY -2
#define BLOCKED -1
static int m[2][MAXS+1][MAXS+1];

/*  numbers on the map!
    -1: no number
    0-  number */
static int mn[MAXS][MAXS];

/*  0,1: horizontal,vertical, 2:interior */
static char touched[3][MAXS][MAXS];  /*  1 if cell is changed and need to be redrawn */

/*  move queue for hint system */
#define MAXMQ MAXS*MAXS*4
static int mq[MAXMQ];
static int mqs,mqe;

static int autosolve;

static int convchar(char *s,int *i) {
  char c=s[(*i)++];
  if(!c) error("string in level definition ended prematurely.");
  else if(c=='.') return -1;
  else if(c>='0') return c-'0';
  error("invalid character %c in level definition.",c);
  return 0;
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
      for(i=j=0;j<x;j++) mn[j][ln]=convchar(s,&i);
      ln++;
    }
  }
  fclose(f);
  startx=10,starty=30;
  mqs=mqe=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) touched[2][i][j]=0;
  for(i=0;i<=x;i++) for(j=0;j<=y;j++) m[0][i][j]=m[1][i][j]=EMPTY;
  for(i=0;i<x;i++) for(j=0;j<=y;j++) m[0][i][j]=UNFILLED,touched[0][i][j]=0;
  for(i=0;i<=x;i++) for(j=0;j<y;j++) m[1][i][j]=UNFILLED,touched[1][i][j]=0;
}

/*  the 4 neighbours to a dot */
static int ddd[4]={1,0,1,0};
static int ddx[4]={0,0,0,-1};
static int ddy[4]={-1,0,0,0};
/*  corner coordinates */
static int ccx[]={0,1,1,0};
static int ccy[]={0,0,1,1};

/*  BFS stuff */
static uchar visit[2][MAXS+1][MAXS+1];
static int qs,qe,q[(MAXS+1)*(MAXS+1)*3];

static void initbfs() {
  qs=qe=0;
  memset(visit,0,sizeof(visit));
}

static int dd2[2][6]={{ 1, 0,1, 1, 0, 1},{ 1, 0,0, 0,1,0}};
static int dx2[2][6]={{ 0,-1,0, 1, 1, 1},{ 0,-1,0,-1,0,0}};
static int dy2[2][6]={{-1, 0,0,-1, 0, 0},{-1, 0,0, 1,1,1}};

/*  given an edge, count number of adjacent edges */
static int countneighbouringedges(int cd,int cx,int cy) {
  int n=0,i,d2,x2,y2;
  for(i=0;i<6;i++) {
    d2=dd2[cd][i];
    x2=cx+dx2[cd][i];
    y2=cy+dy2[cd][i];
    if(x2<0 || y2<0) continue;
    if(d2==0 && x2>=x) continue;
    if(d2==1 && x2>x) continue;
    if(d2==1 && y2>=y) continue;
    if(d2==0 && y2>y) continue;
    if(m[d2][x2][y2]==BLOCKED) n++;
  }
  return n;
}

/*  count number of edges next to dot */
static int edgesneardot(int cx,int cy) {
  int count=0,i,d2,x2,y2;
  for(i=0;i<4;i++) {
    d2=ddd[i];
    x2=cx+ddx[i];
    y2=cy+ddy[i];
    if(x2>=0 && y2>=0 && x2<x+d2 && y2<y+1-d2 && m[d2][x2][y2]==BLOCKED) count++;
  }
  return count;
}

/*  routine which follows the loop
    type 0: follow edges linearly, no bfs
    type 1: follow edges, proper bfs
    returns: 1 if closed loop, 0 if not (for type 0) */
static int genericbfs(int sd,int sx,int sy,int type) {
  int i,cd,cx,cy,d2,x2,y2,vis=0;
  if(visit[sd][sx][sy]) return 0;
  if(type==0 && m[sd][sx][sy]!=BLOCKED) return 0;
  if(type==1 && m[sd][sx][sy]!=BLOCKED) return 0;
  q[qe++]=sd; q[qe++]=sx; q[qe++]=sy;
  visit[sd][sx][sy]=1;
  while(qs<qe) {
    cd=q[qs++]; cx=q[qs++]; cy=q[qs++];
    for(vis=i=0;i<6;i++) {
      d2=dd2[cd][i];
      x2=cx+dx2[cd][i];
      y2=cy+dy2[cd][i];
      if(x2<0 || y2<0) continue;
      if(d2==0 && x2>=x) continue;
      if(d2==1 && x2>x) continue;
      if(d2==1 && y2>=y) continue;
      if(d2==0 && y2>y) continue;
      if(visit[d2][x2][y2]) { vis++; continue; }
      if(type==0 && m[d2][x2][y2]!=BLOCKED) continue;
      if(type==1 && m[d2][x2][y2]!=BLOCKED) continue;
      q[qe++]=d2; q[qe++]=x2; q[qe++]=y2;
      visit[d2][x2][y2]=1;
      if(type==0) break;
    }
  }
  return vis==2;
}

static void cleanupbfs() {
  while(qe) visit[q[qe-3]][q[qe-2]][q[qe-1]]=0,qe-=3;
  qs=0;
}

#define UPDATEVAR(x) if((x)==UNFILLED) unfilled++; else if((x)==EMPTY) empty++; else if((x)==BLOCKED) edge++;
/*  check if cells are violated: -1 (error)
    check if there are points with more than 2 adjacent edges: -1 (error)
    check if there is a closed loop not containing all edges: - 1 (error)
    check for unfulfilled cells: 0 (unfinished)
    otherwise, 1 (solved) */
static int verifyboard() {
  int ok=1,i,j,unfilled,empty,edge,c=0,loops=0,loose=0;
  /*  check for violated cells */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]>-1) {
    empty=unfilled=edge=0;
    UPDATEVAR(m[0][i][j])
    UPDATEVAR(m[0][i][j+1])
    UPDATEVAR(m[1][i][j])
    UPDATEVAR(m[1][i+1][j])
    if(edge>mn[i][j] || unfilled+edge<mn[i][j]) return -1;
    if(edge<mn[i][j] && ok>0) ok=0;
    c=0;
    if(i && m[0][i-1][j]==BLOCKED) c++;
    if(j && m[1][i][j-1]==BLOCKED) c++;
    if(i<x && m[0][i][j]==BLOCKED) c++;
    if(j<y && m[1][i][j]==BLOCKED) c++;
    if(c>2) return -1;
    else if(c==1 && ok>0) ok=0;
  }
  /*  find loose end, horizontal */
  for(i=0;i<x;i++) for(j=0;j<=y;j++) if(m[0][i][j]==BLOCKED && !visit[0][i][j]) {
    /*  only start search from here if cell has 1 neighbour */
    if(countneighbouringedges(0,i,j)!=1) continue;
    genericbfs(0,i,j,0);
    loose++;
  }
  /*  check vertical */
  for(i=0;i<=x;i++) for(j=0;j<y;j++) if(m[1][i][j]==BLOCKED && !visit[1][i][j]) {
    if(countneighbouringedges(1,i,j)!=1) continue;
    genericbfs(1,i,j,0);
    loose++;
  }
  /*  find loops */
  for(i=0;i<x;i++) for(j=0;j<=y;j++) if(m[0][i][j]==BLOCKED && !visit[0][i][j]) {
    if(genericbfs(0,i,j,0)) loops++;
    if(loops>1 || (loops && loose)) goto out;
  }
  for(i=0;i<=x;i++) for(j=0;j<y;j++) if(m[1][i][j]==BLOCKED && !visit[1][i][j]) {
    if(genericbfs(0,i,j,0)) loops++;
    if(loops>1 || (loops && loose)) goto out;
  }
out:
  cleanupbfs();
  if(loops>1 || (loops && loose)) return -1;
  if(!loops) ok=0;
  return ok;
}

/*  redraw the interior of a cell (number or blank) */
static void updatecell(int u,int v) {
  int unfilled=0,empty=0,edge=0;
  Uint32 col=blankcol;
  /*  check neighbours */
  UPDATEVAR(m[0][u][v])
  UPDATEVAR(m[0][u][v+1])
  UPDATEVAR(m[1][u][v])
  UPDATEVAR(m[1][u+1][v])
  if(mn[u][v]==-1) col=blankcol;
  else if(!unfilled && edge==mn[u][v]) col=okcol;
  else if(edge>mn[u][v]) col=errorcol;
  else if(!unfilled && edge<mn[u][v]) col=errorcol;
  else if(4-empty<mn[u][v]) col=errorcol;
  else if(edge+unfilled==mn[u][v] || edge==mn[u][v]) col=almostokcol;
  if(mn[u][v]==-1) drawsolidcell32(u,v,blankcol);
  else drawnumbercell32(u,v,mn[u][v],filledcol,filledcol,col);
}

/*  draw horizontal edge (rectangle) */
void drawhedge(int u,int v,Uint32 col) {
  drawrectangle32(startx+width*u+thick+thin,starty+height*v,startx+width*(u+1)-1-thin,starty+height*v+thick-1,col);}

/*  draw vertical edge (rectangle) */
void drawvedge(int u,int v,Uint32 col) {
  drawrectangle32(startx+width*u,starty+height*v+thick+thin,startx+width*u+thick-1,starty+height*(v+1)-1-thin,col);
}

/*  redraw an edge */
static void updateborder(int d,int u,int v,Uint32 argcol) {
  int eks=0,i,posx,posy;
  Uint32 col=0;
  if(m[d][u][v]==UNFILLED) col=blankcol;
  else if(m[d][u][v]==BLOCKED) col=argcol;
  else if(m[d][u][v]==EMPTY) col=blankcol,eks=1;
  if(!d) drawhedge(u,v,col);
  else drawvedge(u,v,col);
  /*  draw empty, currently drawn as an x */
  if(eks) {
    if(!d) posx=thick+(width-thick-thick)/2,posy=0;
    else posx=0,posy=thick+(height-thick-thick)/2;
    posx+=startx+width*u;
    posy+=starty+height*v;
    for(i=0;i<thick;i++) {
      drawpixel32(posx+i,posy+i,filledcol);
      drawpixel32(posx+thick-i-1,posy+i,filledcol);
    }
  }
}

static void drawgrid() {
  int i,j;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  clear32(WHITE32);
  updatescale(resx-startx,resy-starty,x,y,thick);
  if(thick) {
    for(i=0;i<=x;i++) for(j=0;j<=y;j++) drawrectangle32(startx+width*i,starty+height*j,startx+width*i+thick-1,starty+height*j+thick-1,BLACK32);
  }
  for(i=0;i<x;i++) for(j=0;j<y;j++) updatecell(i,j);
  for(i=0;i<x;i++) for(j=0;j<=y;j++) updateborder(0,i,j,filledcol);
  for(i=0;i<=x;i++) for(j=0;j<y;j++) updateborder(1,i,j,filledcol);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
}

static void domove(int celld,int cellx,int celly,int val) {
  if(val==m[celld][cellx][celly]) error("logical error, tried to set cell to existing value");
  stackpush(celld); stackpush(cellx); stackpush(celly); stackpush(m[celld][cellx][celly]);
  m[celld][cellx][celly]=val;
  touched[celld][cellx][celly]=1;
}

static void partialredraw() {
  int i,j;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(touched[2][i][j]) updatecell(i,j);
  for(i=0;i<x;i++) for(j=0;j<=y;j++) if(touched[0][i][j]) updateborder(0,i,j,filledcol);
  for(i=0;i<=x;i++) for(j=0;j<y;j++) if(touched[1][i][j]) updateborder(1,i,j,filledcol);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(touched[2][i][j]) {
    sdlrefreshcell(i,j);
    touched[2][i][j]=0;
  }
  for(i=0;i<x;i++) for(j=0;j<=y;j++) if(touched[0][i][j]) {
    SDL_UpdateRect(screen,startx+i*width,starty+j*height,width,thick);
    touched[0][i][j]=0;
  }
  for(i=0;i<=x;i++) for(j=0;j<y;j++) if(touched[1][i][j]) {
    SDL_UpdateRect(screen,startx+i*width,starty+j*height,thick,height);
    touched[1][i][j]=0;
  }
}

static void updatetoscreen(int celld,int cellx,int celly,int visible) {
  /*  affect nearby interiors */
  if(celld==1) {
    if(cellx) touched[2][cellx-1][celly]=1;
    if(cellx<x) touched[2][cellx][celly]=1;
  } else if(celld==0) {
    if(celly) touched[2][cellx][celly-1]=1;
    if(celly<y) touched[2][cellx][celly]=1;
  }
  if(visible) partialredraw();
}

static int togglecell(int val) {
  switch(val) {
  case UNFILLED:
  case EMPTY:
    return BLOCKED;
  case BLOCKED:
    return EMPTY;
  default:
    error("wrong value.");
  }
  return 0;
}

static void processmousedown() {
  int celld,cellx,celly,v=controlscheme_slitherlink,up=0;
  getborder(event_mousex,event_mousey,&celld,&cellx,&celly);
  if(celld<0) return;
  if(celld==0 && (cellx<0 || celly<0 || cellx>=x || celly>y)) return;
  if(celld==1 && (cellx<0 || celly<0 || cellx>x || celly>=y)) return;
  if(event_mousebutton==SDL_BUTTON_LEFT) {
    if(!v) domove(celld,cellx,celly,togglecell(m[celld][cellx][celly])),up=1;
    else if(v==1 && m[celld][cellx][celly]!=BLOCKED) domove(celld,cellx,celly,BLOCKED),up=1;
    else if(v==2 && m[celld][cellx][celly]!=EMPTY) domove(celld,cellx,celly,EMPTY),up=1;
  } else if(event_mousebutton==SDL_BUTTON_RIGHT) {
    if(!v && m[celld][cellx][celly]!=UNFILLED) domove(celld,cellx,celly,UNFILLED),up=1;
    else if(v==1 && m[celld][cellx][celly]!=EMPTY) domove(celld,cellx,celly,EMPTY),up=1;
    else if(v==2 && m[celld][cellx][celly]!=BLOCKED) domove(celld,cellx,celly,BLOCKED),up=1;
  } else if(event_mousebutton==SDL_BUTTON_MIDDLE) {
    if(v && m[celld][cellx][celly]!=UNFILLED) domove(celld,cellx,celly,UNFILLED),up=1;
  }
  if(up) updatetoscreen(celld,cellx,celly,1),numclicks++,normalmove=1;
}

static void undo(int visible) {
  if(!stackempty()) {
    int val=stackpop(),celly=stackpop(),cellx=stackpop(),celld=stackpop();
    m[celld][cellx][celly]=val;
    touched[celld][cellx][celly]=1;
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

/*  surround 0 with empty */
static int level1cleararound0() {
  int i,j,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]==0) {
    if(m[0][i][j]==UNFILLED) addmovetoqueue(0,i,j,EMPTY),ok=1;
    if(m[1][i][j]==UNFILLED) addmovetoqueue(1,i,j,EMPTY),ok=1;
    if(m[0][i][j+1]==UNFILLED) addmovetoqueue(0,i,j+1,EMPTY),ok=1;
    if(m[1][i+1][j]==UNFILLED) addmovetoqueue(1,i+1,j,EMPTY),ok=1;
  }
  return ok;
}

/*  if number has enough edges, fill the rest with empty */
/*  if filling all unfilled with edges results in num==edges, fill with edges */
static int level1filltrivial() {
  int i,j,ok=0,unfilled,empty,edge;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]>-1) {
    unfilled=empty=edge=0;
    UPDATEVAR(m[0][i][j])
    UPDATEVAR(m[0][i][j+1])
    UPDATEVAR(m[1][i][j])
    UPDATEVAR(m[1][i+1][j])
    if(!unfilled) continue;
    if(edge==mn[i][j]) {
      if(m[0][i][j]==UNFILLED) addmovetoqueue(0,i,j,EMPTY),ok=1;
      if(m[1][i][j]==UNFILLED) addmovetoqueue(1,i,j,EMPTY),ok=1;
      if(m[0][i][j+1]==UNFILLED) addmovetoqueue(0,i,j+1,EMPTY),ok=1;
      if(m[1][i+1][j]==UNFILLED) addmovetoqueue(1,i+1,j,EMPTY),ok=1;
    } else if(edge+unfilled==mn[i][j]) {
      if(m[0][i][j]==UNFILLED) addmovetoqueue(0,i,j,BLOCKED),ok=1;
      if(m[1][i][j]==UNFILLED) addmovetoqueue(1,i,j,BLOCKED),ok=1;
      if(m[0][i][j+1]==UNFILLED) addmovetoqueue(0,i,j+1,BLOCKED),ok=1;
      if(m[1][i+1][j]==UNFILLED) addmovetoqueue(1,i+1,j,BLOCKED),ok=1;
    }
  }
  return ok;
}

#define UPDATEVAR2(d,i,j) if(i<0 || j<0 || i>=x+d || j>=y+1-d || m[d][i][j]==EMPTY) empty++; else if(m[d][i][j]==UNFILLED) unfilled++; else if(m[d][i][j]==BLOCKED) edge++;
#define FILLEDGE2(d,i,j,val) if(i>=0 && j>=0 && i<x+d && j<y+1-d && m[d][i][j]==UNFILLED) addmovetoqueue(d,i,j,val),ok=1;
/*  if a dot has 3 empty neighbours, 4th must be empty */
/*  if a dot has 2 edges next to it, the remaining must be empty */
/*  if a dot has 1 edge and 2 empty, 4th must be edge */
static int level1dot() {
  int i,j,ok=0,unfilled,empty,edge;
  for(i=0;i<=x;i++) for(j=0;j<=y;j++) {
    unfilled=empty=edge=0;
    UPDATEVAR2(0,i,j)
    UPDATEVAR2(1,i,j)
    UPDATEVAR2(0,i-1,j)
    UPDATEVAR2(1,i,j-1)
    if(!unfilled) continue;
    if(empty==3 || edge==2) {
      FILLEDGE2(0,i,j,EMPTY)
      FILLEDGE2(1,i,j,EMPTY)
      FILLEDGE2(0,i-1,j,EMPTY)
      FILLEDGE2(1,i,j-1,EMPTY)
    } else if(edge==1 && empty==2) {
      FILLEDGE2(0,i,j,BLOCKED)
      FILLEDGE2(1,i,j,BLOCKED)
      FILLEDGE2(0,i-1,j,BLOCKED)
      FILLEDGE2(1,i,j-1,BLOCKED)
    }
  }
  return ok;
}

/*  two adjacent 3's: if they are horizontal, fill the 3 vertical edges
    if vertical, fill the 3 horizontal edge */
static int level1adjacent3() {
  int i,j,ok=0;
  /*  vertical 3's */
  for(i=0;i<x;i++) for(j=0;j<y-1;j++) if(mn[i][j]==3 && mn[i][j+1]==3) {
    if(m[0][i][j]==UNFILLED) addmovetoqueue(0,i,j,BLOCKED),ok=1;
    if(m[0][i][j+1]==UNFILLED) addmovetoqueue(0,i,j+1,BLOCKED),ok=1;
    if(m[0][i][j+2]==UNFILLED) addmovetoqueue(0,i,j+2,BLOCKED),ok=1;
    if(i && m[0][i-1][j+1]==UNFILLED) addmovetoqueue(0,i-1,j+1,EMPTY),ok=1;
    if(i<x-1 && m[0][i+1][j+1]==UNFILLED) addmovetoqueue(0,i+1,j+1,EMPTY),ok=1;
  }
  /*  horizontal 3's */
  for(i=0;i<x-1;i++) for(j=0;j<y;j++) if(mn[i][j]==3 && mn[i+1][j]==3) {
    if(m[1][i][j]==UNFILLED) addmovetoqueue(1,i,j,BLOCKED),ok=1;
    if(m[1][i+1][j]==UNFILLED) addmovetoqueue(1,i+1,j,BLOCKED),ok=1;
    if(m[1][i+2][j]==UNFILLED) addmovetoqueue(1,i+2,j,BLOCKED),ok=1;
    if(j && m[1][i+1][j-1]==UNFILLED) addmovetoqueue(1,i+1,j-1,EMPTY),ok=1;
    if(j<y-1 && m[1][i+1][j+1]==UNFILLED) addmovetoqueue(1,i+1,j+1,EMPTY),ok=1;
  }
  return ok;
}

static int level1oneedge3() {
  int i,j,ok=0,k,cd,cx,cy,l,ax,ay,ix;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]==3) {
    /*  find a corner with the two outgoing containing one edge */
    for(k=0;k<4;k++) {
      ax=i+ccx[k];
      ay=j+ccy[k];
      for(l=0;l<2;l++) {
        ix=(k+l+3)&3;
        cd=ddd[ix];
        cx=ax+ddx[ix];
        cy=ay+ddy[ix];
        if(cx>=0 && cy>=0 && cx<x+cd && cy<y+1-cd && m[cd][cx][cy]==BLOCKED) {
          ax=i+ccx[k^2];
          ay=j+ccy[k^2];
          for(l=0;l<2;l++) {
            ix=(k+l+3)&3;
            cd=ddd[ix];
            cx=ax+ddx[ix];
            cy=ay+ddy[ix];
            if(cx>=0 && cy>=0 && cx<x+cd && cy<y+1-cd && m[cd][cx][cy]==UNFILLED)
              addmovetoqueue(cd,cx,cy,BLOCKED),ok=1;
          }
          goto next;
        }
      }
    next:;
    }
  }
  return ok;
}

static int level1emptycorner3() {
  int i,j,ok=0,k,ax,ay,cd,cx,cy,l,ix,count;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]==3) {
    /*  find a corner with the two outgoing edges being empty */
    for(k=0;k<4;k++) {
      ax=i+ccx[k];
      ay=j+ccy[k];
      for(count=l=0;l<2;l++) {
        ix=(k+l+3)&3;
        cd=ddd[ix];
        cx=ax+ddx[ix];
        cy=ay+ddy[ix];
        if(cx<0 || cy<0 || cx>=x+cd || cy>=y+1-cd || m[cd][cx][cy]==EMPTY) count++;
      }
      if(count==2) for(l=2;l<4;l++) {
        ix=(k+l+3)&3;
        cd=ddd[ix];
        cx=ax+ddx[ix];
        cy=ay+ddy[ix];
        if(cx>=0 && cy>=0 && cx<x+cd && cy<y+1-cd && m[cd][cx][cy]==UNFILLED)
          addmovetoqueue(cd,cx,cy,BLOCKED),ok=1;
      }
    }
  }
  return ok;
}

static int level1emptycorner1() {
  int i,j,ok=0,k,ax,ay,cd,cx,cy,l,ix,count;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]==1) {
    /*  find a corner with the two outgoing edges being empty */
    for(k=0;k<4;k++) {
      ax=i+ccx[k];
      ay=j+ccy[k];
      for(count=l=0;l<2;l++) {
        ix=(k+l+3)&3;
        cd=ddd[ix];
        cx=ax+ddx[ix];
        cy=ay+ddy[ix];
        if(cx<0 || cy<0 || cx>=x+cd || cy>=y+1-cd || m[cd][cx][cy]==EMPTY) count++;
      }
      if(count==2) for(l=2;l<4;l++) {
        ix=(k+l+3)&3;
        cd=ddd[ix];
        cx=ax+ddx[ix];
        cy=ay+ddy[ix];
        if(cx>=0 && cy>=0 && cx<x+cd && cy<y+1-cd && m[cd][cx][cy]==UNFILLED)
          addmovetoqueue(cd,cx,cy,EMPTY),ok=1;
      }
    }
  }
  return ok;
}

static int level1diagonal3() {
  int i,j,ok=0;
  for(i=0;i<x-1;i++) for(j=0;j<y-1;j++) if(mn[i][j]==3 && mn[i+1][j+1]==3) {
    if(m[0][i][j]==UNFILLED) addmovetoqueue(0,i,j,BLOCKED),ok=1;
    if(m[1][i][j]==UNFILLED) addmovetoqueue(1,i,j,BLOCKED),ok=1;
    if(m[0][i+1][j+2]==UNFILLED) addmovetoqueue(0,i+1,j+2,BLOCKED),ok=1;
    if(m[1][i+2][j+1]==UNFILLED) addmovetoqueue(1,i+2,j+1,BLOCKED),ok=1;
  } else if(mn[i][j+1]==3 && mn[i+1][j]==3) {
    if(m[0][i+1][j]==UNFILLED) addmovetoqueue(0,i+1,j,BLOCKED),ok=1;
    if(m[1][i+2][j]==UNFILLED) addmovetoqueue(1,i+2,j,BLOCKED),ok=1;
    if(m[0][i][j+2]==UNFILLED) addmovetoqueue(0,i,j+2,BLOCKED),ok=1;
    if(m[1][i][j+1]==UNFILLED) addmovetoqueue(1,i,j+1,BLOCKED),ok=1;
  }
  return ok;
}

/*  adjacent 1 and 3 near border: edge between 3 and border */
static int level1border31() {
  int i,ok=0;
  for(i=0;i<x-1;i++) {
    if(mn[i][0]==3 && mn[i+1][0]==1 && m[0][i][0]==UNFILLED) addmovetoqueue(0,i,0,BLOCKED),ok=1;
    if(mn[i][0]==1 && mn[i+1][0]==3 && m[0][i+1][0]==UNFILLED) addmovetoqueue(0,i+1,0,BLOCKED),ok=1;
    if(mn[i][y-1]==3 && mn[i+1][y-1]==1 && m[0][i][y]==UNFILLED) addmovetoqueue(0,i,y,BLOCKED),ok=1;
    if(mn[i][y-1]==1 && mn[i+1][y-1]==3 && m[0][i+1][y]==UNFILLED) addmovetoqueue(0,i+1,y,BLOCKED),ok=1;
  }
  for(i=0;i<y-1;i++) {
    if(mn[0][i]==3 && mn[0][i+1]==1 && m[1][0][i]==UNFILLED) addmovetoqueue(1,0,i,BLOCKED),ok=1;
    if(mn[0][i]==1 && mn[0][i+1]==3 && m[1][0][i+1]==UNFILLED) addmovetoqueue(1,0,i+1,BLOCKED),ok=1;
    if(mn[x-1][i]==3 && mn[x-1][i+1]==1 && m[1][x][i]==UNFILLED) addmovetoqueue(1,x,i,BLOCKED),ok=1;
    if(mn[x-1][i]==1 && mn[x-1][i+1]==3 && m[1][x][i+1]==UNFILLED) addmovetoqueue(1,x,i+1,BLOCKED),ok=1;
  }
  return ok;
}

/*  2 in corner */
static int level1corner2() {
  int ok=0;
  /*  2 */
  if(mn[0][0]==2) {
    if(m[0][1][0]==UNFILLED) addmovetoqueue(0,1,0,BLOCKED),ok=1;
    if(m[1][0][1]==UNFILLED) addmovetoqueue(1,0,1,BLOCKED),ok=1;
  }
  if(mn[x-1][0]==2) {
    if(m[0][x-2][0]==UNFILLED) addmovetoqueue(0,x-2,0,BLOCKED),ok=1;
    if(m[1][x][1]==UNFILLED) addmovetoqueue(1,x,1,BLOCKED),ok=1;
  }
  if(mn[0][y-1]==2) {
    if(m[0][1][y]==UNFILLED) addmovetoqueue(0,1,y,BLOCKED),ok=1;
    if(m[1][0][y-2]==UNFILLED) addmovetoqueue(1,0,y-2,BLOCKED),ok=1;
  }
  if(mn[x-1][y-1]==2) {
    if(m[0][x-2][y]==UNFILLED) addmovetoqueue(0,x-2,y,BLOCKED),ok=1;
    if(m[1][x][y-2]==UNFILLED) addmovetoqueue(1,x,y-2,BLOCKED),ok=1;
  }
  return ok;
}

static int level1hint() {
  if(level1cleararound0()) return 1;
  if(level1adjacent3()) return 1;
  if(level1border31()) return 1;
  if(level1corner2()) return 1;
  if(level1diagonal3()) return 1;
  if(level1filltrivial()) return 1;
  if(level1dot()) return 1;
  if(level1oneedge3()) return 1;
  if(level1emptycorner3()) return 1;
  if(level1emptycorner1()) return 1;
  return 0;
}

static int level2jordancurve() {
  int ok=0,i,j,unfilled,flip,pos=0,fill;
  /*  check every column */
  for(i=0;i<x;i++) {
    for(flip=unfilled=j=0;j<=y && unfilled<2;j++) if(m[0][i][j]==UNFILLED) unfilled++,pos=j;
    else if(m[0][i][j]==BLOCKED) flip^=1;
    if(unfilled==1) {
      fill=flip?BLOCKED:EMPTY;
      if(m[0][i][pos]==UNFILLED) addmovetoqueue(0,i,pos,fill),ok=1;
    }
  }
  /*  check every row */
  for(i=0;i<y;i++) {
    for(flip=unfilled=j=0;j<=x && unfilled<2;j++) if(m[1][j][i]==UNFILLED) unfilled++,pos=j;
    else if(m[1][j][i]==BLOCKED) flip^=1;
    if(unfilled==1) {
      fill=flip?BLOCKED:EMPTY;
      if(m[1][pos][i]==UNFILLED) addmovetoqueue(1,pos,i,fill),ok=1;
    }
  }
  return ok;
}

static int level2hint() {
  if(level2jordancurve()) return 1;
  return 0;
}

static int level3hint() {
  return 0;
}

static int level4avoidloop() {
  int i,j;
  /*  horizontal lines */
  for(i=0;i<x;i++) for(j=0;j<=y;j++) if(m[0][i][j]==UNFILLED && edgesneardot(i,j)==1 && edgesneardot(i+1,j)==1) {
    m[0][i][j]=BLOCKED;
    if(verifyboard()<0) {
      m[0][i][j]=UNFILLED;
      addmovetoqueue(0,i,j,EMPTY);
      return 1;
    }
    m[0][i][j]=UNFILLED;
  }
  /*  vertical lines */
  for(i=0;i<=x;i++) for(j=0;j<y;j++) if(m[1][i][j]==UNFILLED && edgesneardot(i,j)==1 && edgesneardot(i,j+1)==1) {
    m[1][i][j]=BLOCKED;
    if(verifyboard()<0) {
      m[1][i][j]=UNFILLED;
      addmovetoqueue(1,i,j,EMPTY);
      return 1;
    }
    m[1][i][j]=UNFILLED;
  }
  return 0;
}

static int level4hint() {
  if(level4avoidloop()) return 1;
  return 0;
}

static int lev5alt1[2][MAXS+1][MAXS+1];
static int lev5alt2[2][MAXS+1][MAXS+1];

/*  copy board from a to b */
static void copyboard(int a[2][MAXS+1][MAXS+1],int b[2][MAXS+1][MAXS+1]) {
  int i,j,k;
  for(k=0;k<2;k++) for(i=0;i<=x;i++) for(j=0;j<=y;j++) b[k][i][j]=a[k][i][j];
}

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

static int level5contradiction(int lev) {
  static int i=0,j=0,dir=0;
  int z=2*(x+1)*(y+1),r,k,l,oldsp=getstackpos(),q;
	if(i>x) i=0; if(j>y) j=0;
  while(z--) {
    if(m[dir][i][j]==UNFILLED) {
      /*  assume blocked */
      domove(dir,i,j,BLOCKED);
      updatetoscreen(dir,i,j,0);
      r=dogreedy(lev);
      if(r<0) {
        /*  contradiction! */
        while(getstackpos()>oldsp) undo(0);
        addmovetoqueue(dir,i,j,EMPTY);
        return 1;
      }
      copyboard(m,lev5alt1);
      while(getstackpos()>oldsp) undo(0);
      /*  assume empty */
      domove(dir,i,j,EMPTY);
      updatetoscreen(dir,i,j,0);
      r=dogreedy(lev);
      if(r<0) {
        /*  contradiction! */
        while(getstackpos()>oldsp) undo(0);
        addmovetoqueue(dir,i,j,BLOCKED);
        return 1;
      }
      copyboard(m,lev5alt2);
      while(getstackpos()>oldsp) undo(0);
      for(r=q=0;q<2;q++) for(k=0;k<=x;k++) for(l=0;l<=y;l++) if(lev5alt1[q][k][l]!=UNFILLED && lev5alt1[q][k][l]==lev5alt2[q][k][l] && m[q][k][l]==UNFILLED)
        addmovetoqueue(q,k,l,lev5alt1[q][k][l]),r=1;
      if(r) return 1;
    }
    j++;
    if(j==y+1) {
      j=0; i++;
      if(i==x+1) {
        i=0;
        dir++;
        if(dir==2) dir=0;
      }
    }
  }
  return 0;
}

static int level5hint() {
  if(level5contradiction(3)) return 1;
  if(level5contradiction(4)) return 1;
  return 0;
}

static int hint() {
	usedhint=1;
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
  logprintf("trol\n");
  if(forcefullredraw) drawgrid();
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<=y;j++) if(!visit[0][i][j] && m[0][i][j]==BLOCKED) {
    qs2=qs;
    genericbfs(0,i,j,1);
    for(k=qs2;k<qe;k+=3) updateborder(q[k],q[k+1],q[k+2],colarray[col]);
    col=(col+1)%COLSIZE;
  }
  for(i=0;i<=x;i++) for(j=0;j<y;j++) if(!visit[1][i][j] && m[1][i][j]==BLOCKED) {
    qs2=qs;
    genericbfs(1,i,j,1);
    for(k=qs2;k<qe;k+=3) updateborder(q[k],q[k+1],q[k+2],colarray[col]);
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
  for(i=0;i<x;i++) for(j=0;j<=y;j++) updateborder(0,i,j,filledcol);
  for(i=0;i<=x;i++) for(j=0;j<y;j++) updateborder(1,i,j,filledcol);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
}

#undef COLSIZE

static void processkeydown(int key) {
  int res;
  if(key==undokey) undo(1),usedundo=1;
  else if(key==verifykey) showverify();
  else if(key==hintkey) {
    if(!executeonemovefromqueue(1)) {
      res=hint();
      if(res>0) executeonemovefromqueue(1);
      else if(!res) messagebox(1,"Sorry, no moves found.");
      else messagebox(1,"Sorry, hint will not work on an illegal board.");
    }
  } else if(key==SDLK_j) {  /*  temporary: superhintkey */
    res=hint();
    if(res>0) {
      executemovequeue();
      while(hint()>0) executemovequeue();
      if(verifyboard()<1) messagebox(1,"Sorry, no more moves found.");
    } else if(!res) messagebox(1,"Sorry, no moves found.");
    else messagebox(1,"Sorry, hint will not work on an illegal board.");
  }
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

void slitherlink(char *path,int solve) {
  int event,oldthick=thick;
  autosolve=solve;
  if(slitherlinkthick>-1) thick=slitherlinkthick;
  loadpuzzle(path);
  initbfs();
  drawgrid();
  if(autosolve) { autosolver(path); goto done; }
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
        goto done;
      }
      break;
    default:
      /*  catch intervals of values here */
      if(event>=EVENT_KEYDOWN && event<EVENT_KEYUP) {
        processkeydown(event-EVENT_KEYDOWN);
        if(verifyboard()>0) {
					finalizetime(); displayscore(x,y);
          messagebox(1,"You are winner!");
          goto done;
        }
      }
    }
  } while(event!=EVENT_QUIT && !keys[SDLK_ESCAPE]);
done:
	thick=oldthick;
}
