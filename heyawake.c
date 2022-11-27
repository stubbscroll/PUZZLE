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

/*  warning, must have UNFILLED<EMPTY<BLOCKED */
#define UNFILLED -3
#define EMPTY -2
#define BLOCKED -1
/*  internal format:
    -3:   unfilled cell
    -2:   empty cell
    -1:   blocked cell
*/
static int m[MAXS][MAXS];
/*  mn[][]:
    -1:   no number
    0-:   number */
static int mn[MAXS][MAXS];
/*  specify whether the top or left edge of the cell is thicker */
static char topedge[MAXS][MAXS],leftedge[MAXS][MAXS];

/*  regions: mark which region each cell belongs to */
int region[MAXS][MAXS];
/*  number of regions */
int regn;
/*  region coordinates */
int regx[MAXS*MAXS],regy[MAXS*MAXS];
/*  region i is represented in the array above with indexes in [regs[i], regs[i+1]). */
/*  hence, size of region i is regs[i+1]-regs[i] */
#define REGSIZE(i) (regs[(i)+1]-regs[(i)])
int regs[MAXS*MAXS+1];

static int autosolve;

/*  keep helper arrays for regions */
/*  these will always be kept up to date, so it is safe for verifyboard, solver etc to use them */
static int regnum[MAXS*MAXS];   /*  the number in this region, or -1 if none */
static char isrect[MAXS*MAXS];  /*  mark whether region is rectangular: 1 if yes */
static int regsizex[MAXS*MAXS],regsizey[MAXS*MAXS];   /*  size of region (if rectangular) */
static int regupperx[MAXS*MAXS],reguppery[MAXS*MAXS]; /*  upper left coordinate of region (if rectangular) */
/*  the following are the only ones that will change after init */
static int regempty[MAXS*MAXS]; /*  number of empty in this region */
static int regblock[MAXS*MAXS]; /*  number of blocked in this region */

/*  given a region number, return its colour */
static Uint32 regst(int no) {
  /*  unnumbered region has no unfilled cells: ok */
  if(regnum[no]<0 && regempty[no]+regblock[no]==REGSIZE(no)) return okcol;
  /*  numbered region with correct number of blocked and no unfilled: ok */
  if(regnum[no]>-1 && regempty[no]+regblock[no]==REGSIZE(no) && regnum[no]==regblock[no]) return okcol;
  /*  numbered region with correct number of blocks and unfilled: "almost ok", toclose */
  if(regnum[no]>-1 && regnum[no]==regblock[no]) return almostokcol;
  /*  numbered region with number of blocks + unfilled = val, "almost ok" */
  if(regnum[no]==REGSIZE(no)-regempty[no]) return almostokcol;
  /*  numbered region with blocks>value: errorcol */
  if(regnum[no]>-1 && regnum[no]<regblock[no]) return errorcol;
  /*  no unfilled, blocked!=val */
  if(regnum[no]>-1 && regnum[no]!=regblock[no] && regblock[no]+regempty[no]==REGSIZE(no)) return errorcol;
  /*  otherwise, blankcol */
  return blankcol;
}

/*  st[][]: bit 0 is set if there is a horizontal strip crossing >=2 borders,
            bit 1 is set if there is a vertical strip crossing >=2 borders
    so, if st[][]>0, cell is shown red */
static int st[MAXS][MAXS];

static char touched[MAXS][MAXS];  /*  1 if cell is changed and need to be redrawn */

/*  move queue for hint system */
#define MAXMQ MAXS*MAXS*3
static int mq[MAXMQ];
static int mqs,mqe;

/*  determine the regions. reuse touched[][] and mq[], but reset them afterwards */
#define PROCESS(x2,y2) { touched[x2][y2]=1; \
  region[x2][y2]=regn; \
  regx[ix]=x2; regy[ix++]=y2; \
  mq[mqe++]=x2; mq[mqe++]=y2; }
void findregions() {
  int i,j,cx,cy,ix=0,minx,miny,maxx,maxy,sx,sy,k;
  regn=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!touched[i][j]) {
    regs[regn]=ix;
    region[i][j]=regn;
    regx[ix]=i; regy[ix++]=j;
    mqs=mqe=0;
    mq[mqe++]=i; mq[mqe++]=j;
    touched[i][j]=1;
    while(mqs<mqe) {
      cx=mq[mqs++]; cy=mq[mqs++];
      if(!topedge[cx][cy] && !touched[cx][cy-1]) PROCESS(cx,cy-1)
      if(!leftedge[cx][cy] && !touched[cx-1][cy]) PROCESS(cx-1,cy)
      if(!topedge[cx][cy+1] && !touched[cx][cy+1]) PROCESS(cx,cy+1)
      if(!leftedge[cx+1][cy] && !touched[cx+1][cy]) PROCESS(cx+1,cy)
    }
    minx=MAXS; maxx=0;
    miny=MAXS; maxy=0;
    for(k=0;k<mqe;k+=2) {
      cx=mq[k]; cy=mq[k+1];
      if(minx>cx) minx=cx;
      if(maxx<cx) maxx=cx;
      if(miny>cy) miny=cy;
      if(maxy<cy) maxy=cy;
    }
    sx=maxx-minx+1; sy=maxy-miny+1;
    if(mqe==sx*sy*2) {
      isrect[regn]=1;
      regsizex[regn]=sx;
      regsizey[regn]=sy;
      regupperx[regn]=minx;
      reguppery[regn]=miny;
    } else isrect[regn]=0;
    regn++;
  }
  regs[regn]=ix;
  for(i=0;i<regn;i++) regempty[i]=regblock[i]=0,regnum[i]=-1;
  /*  reset */
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    st[i][j]=touched[i][j]=0;
    if(mn[i][j]>-1) regnum[region[i][j]]=mn[i][j];
  }
  mqs=mqe=0;
}
#undef PROCESS

/*  pascal's triangle! */
#define MAXP 200
#define INF 1000000000
int pascal[MAXP][MAXP];
void genpascal() {
  int i,j;
  for(i=0;i<MAXP;i++) {
    pascal[i][0]=pascal[i][i]=0;
    for(j=1;j<i;j++) {
      pascal[i][j]=pascal[i-1][j]-pascal[i-1][j-1];
      if(pascal[i][j]>INF) pascal[i][j]=INF;
    }
  }
}

static int convchar(char *s,int *i) {
  int v;
  char c=s[(*i)++];
  if(!c) error("string in level definition ended prematurely.");
  else if(c=='.') return UNFILLED;
  else if(c>='0' && c<='9') return c-'0';
  else if(c>='a' && c<='z') return c-'a'+10;
  else if(c>='A' && c<='Z') return c-'A'+36;
  else if(c=='{') {
    v=0;
    while(s[*i] && s[*i]!='}') v=v*10+s[(*i)++]-48;
    if(s[*i]=='}') (*i)++;
    return v;
  }
  error("invalid character %c in level definition.",c);
  return 0;
}

static int convborder(char *s,int *i) {
  char c=s[(*i)++];
  if(!c) error("string in level definition ended prematurely.");
  return c=='|' || c=='-';
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
      sscanf(s,"%d %d",&x,&y);
      z++;
      break;
    case 3:
      /*  read horizontal border */
      for(i=j=0;j<=x;j++) {
        convborder(s,&i);
        topedge[j][ln]=convborder(s,&i);
      }
      if(ln==y) goto done;
      z++;
      break;
    case 4:
      /*  read contents */
      for(i=j=0;j<x;j++) {
        leftedge[j][ln]=convborder(s,&i);
        m[j][ln]=convchar(s,&i);
      }
      leftedge[j][ln++]=convborder(s,&i);
      z--;
      break;
    }
  } else if(!gameinfo[0]) strcpy(gameinfo,s+2);
done:
  fclose(f);
  startx=10,starty=(int)(font->height*2.5);
  mqs=mqe=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    mn[i][j]=-1;
    if(m[i][j]>-1) {
      mn[i][j]=m[i][j];
      m[i][j]=UNFILLED;
    }
    touched[i][j]=0;
  }
  findregions();
}

/*  utility functions! */

/*  count the number of crossings from this tile in a given direction */
static int countcrossings(int cellx,int celly,int dx,int dy) {
  int res=0;
  if(m[cellx][celly]!=EMPTY) return 0;
  while(1) {
    cellx+=dx; celly+=dy;
    if(cellx<0 || celly<0 || cellx>=x || celly>=y || m[cellx][celly]!=EMPTY) return res;
    if(dx<0) res+=leftedge[cellx+1][celly];
    else if(dx>0) res+=leftedge[cellx][celly];
    if(dy<0) res+=topedge[cellx][celly+1];
    else if(dy>0) res+=topedge[cellx][celly];
  }
}

/*  update st[][] in a given direction. op=0 is or (add), op=1 is and (remove) */
static void updatestdir(int cellx,int celly,int dx,int dy,int op,int mask) {
  while(1) {
    if(cellx<0 || celly<0 || cellx>=x || celly>=y || m[cellx][celly]!=EMPTY) return;
    if(!op && !(st[cellx][celly]&mask)) {
      st[cellx][celly]|=mask;
      touched[cellx][celly]=1;
    } else if(op && (st[cellx][celly]&mask)) {
      st[cellx][celly]&=~mask;
      touched[cellx][celly]=1;
    }
    cellx+=dx; celly+=dy;
  }
}

/*  check if the cell has blocked next to it */
static int hasadjacentblocked(int cellx,int celly) {
  int i,cx,cy;
  for(i=0;i<4;i++) {
    cx=cellx+dx[i]; cy=celly+dy[i];
    if(cx<0 || cy<0 || cx>=x || cy>=y || m[cx][cy]!=BLOCKED) continue;
    return 1;
  }
  return 0;
}

/*  BFS stuff */
static uchar visit[MAXS][MAXS];
static int qs,qe,q[MAXS*MAXS*2];

static void initbfs() {
  qs=qe=0;
  memset(visit,0,sizeof(visit));
}

/* generic bfs!
   type 0: search for empty & unfilled
   type 1: search through empty only
   returns: number of cells examined  */
static int genericbfs(int sx,int sy,int type) {
  int tot=1,i,cx,cy,x2,y2;
  if(visit[sx][sy]) return 0;
  if(type==0 && m[sx][sy]>=BLOCKED) return 0;
	if(type==1 && m[sx][sy]!=EMPTY) return 0;
  q[qe++]=sx; q[qe++]=sy;
  visit[sx][sy]=1;
  while(qs<qe) {
    cx=q[qs++]; cy=q[qs++];
    for(i=0;i<4;i++) {
      x2=cx+dx[i],y2=cy+dy[i];
      if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2]) continue;
      if(type==0 && m[x2][y2]>=BLOCKED) continue;
			if(type==1 && m[x2][y2]!=EMPTY) continue;
      tot++;
      q[qe++]=x2,q[qe++]=y2;
      visit[x2][y2]=1;
    }
  }
  return tot;
}

static void cleanupbfs() {
  while(qe) visit[q[qe-2]][q[qe-1]]=0,qe-=2;
  qs=0;
}

#define OUTOFBOUNDS -10000
static int readcell(int cx,int cy) {
  if(cx<0 || cy<0 || cx>=x || cy>=y) return OUTOFBOUNDS;
  return m[cx][cy];
}

/*  overridecol>=0: force colour, override<0: normal colour */
static void updatecell(int u,int v,int override) {
  Uint32 bk,col;
  int up,down,left,right;
  /*  calculate border sizes */
  up=topedge[u][v],down=topedge[u][v+1],left=leftedge[u][v],right=leftedge[u+1][v];
  up=!up?thin:up*(thick-1)/2;
  down=!down?0:down*thick/2;
  left=!left?thin:left*(thick-1)/2;
  right=!right?0:right*thick/2;
  /*  determine colour */
  if(m[u][v]==BLOCKED) {
    bk=hasadjacentblocked(u,v)?darkererrorcol:filled2col;
    if(mn[u][v]>-1) col=regst(region[u][v]); else col=WHITE32;
  } else if(m[u][v]==UNFILLED) bk=override>-1?override:unfilledcol,col=BLACK32;
  else {
    bk=override>-1?override:(st[u][v]?errorcol:regst(region[u][v]));
    col=BLACK32;
  }
  /*  determine if number */
  if(mn[u][v]<0) drawsolidcell32w(u,v,bk,left,up,right,down);
  else drawnumbercell32w(u,v,mn[u][v],col,col,bk,left,up,right,down);
  /*  plot border */
  if(up) drawrectangle32(startx+u*width,starty+v*height,startx+(u+1)*width,starty+v*height+up-1,BLACK32);
  if(left) drawrectangle32(startx+u*width,starty+v*height,startx+u*width+left-1,starty+(v+1)*height,BLACK32);
  if(down) drawrectangle32(startx+u*width,starty+(v+1)*height-down,startx+(u+1)*width,starty+(v+1)*height-1,BLACK32);
  if(right) drawrectangle32(startx+(u+1)*width-right,starty+v*height,startx+(u+1)*width-1,starty+(v+1)*height,BLACK32);
}

static void drawgrid() {
  int i,j,left,up;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  clear32(WHITE32);
  updatescale(resx-startx,resy-starty,x,y,thick);
  if(thin) {
    for(i=1;i<y;i++) for(j=0;j<thin;j++) drawhorizontalline32(startx,startx+width*x+thin-1,starty+i*height+j,BLACK32);
    for(i=1;i<x;i++) drawrectangle32(startx+width*i,starty,startx+i*width+thin-1,starty+y*height+thin-1,BLACK32);
  }
  /*  outer border */
  if(thick) {
    left=startx-thick/2; if(left<0) left=0;
    up=starty-thick/2; if(up<0) up=0;
    /*  left */
    drawrectangle32(left,up,startx-1,starty+y*height+(thick-1)/2-1,BLACK32);
    /*  up */
    drawrectangle32(left,up,startx+x*width+(thick-1)/2-1,starty-1,BLACK32);
    /*  bottom */
    drawrectangle32(left,starty+y*height,startx+x*width+(thick-1)/2-1,starty+y*height+(thick-1)/2-1,BLACK32);
    /*  right */
    drawrectangle32(startx+x*width,up,startx+x*width+(thick-1)/2-1,starty+y*height+(thick-1)/2-1,BLACK32);
  }
  for(i=0;i<x;i++) for(j=0;j<y;j++) updatecell(i,j,-1);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
}

/*  return 1 if solved, 0 if unsolved, -1 if illegal */
static int verifyboard() {
  int i,j,emptyreg=0;
  /*  check for adjacent blocks */
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    if(i<x-1 && m[i][j]==BLOCKED && m[i+1][j]==BLOCKED) return -1;
    if(j<y-1 && m[i][j]==BLOCKED && m[i][j+1]==BLOCKED) return -1;
  }
  /*  check for empty strips crossing >=2 borders */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(st[i][j]) return -1;
  /*  check for regions that aren't possible to fulfill */
  for(i=0;i<regn;i++) if(regnum[i]>-1)  {
    if(regblock[i]>regnum[i]) return -1;
    if(REGSIZE(i)-regempty[i]<regnum[i]) return -1;
  }
  /*  check if empty area is disconnected */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && m[i][j]<BLOCKED) {
    if(++emptyreg>1) goto out;
    genericbfs(i,j,0);
  }
out:
  cleanupbfs();
  if(emptyreg!=1) return -1;
  /*  now, check if puzzle is solved */
  for(i=0;i<regn;i++) {
    if(regnum[i]>-1 && regnum[i]!=regblock[i]) return 0;
    if(REGSIZE(i)!=regblock[i]+regempty[i]) return 0;
  }
  return 1;
}

static void partialredraw() {
  int i,j;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(touched[i][j]) updatecell(i,j,-1);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(touched[i][j]) {
    sdlrefreshcell(i,j);
    touched[i][j]=0;
  }
}

/*  do some fancy colour magic when updating:
    - strips spanning three regions becomes red
    - unnumbered regions with no unfilled cell becomes green
    - numbered regions with no unfilled cell and blocked==number becomes green
    - numbered regions with blocked>number becomes red
    - numbered regions with blocked==number containing unfilled becomes yellow
    - otherwise, white
*/
static uchar oldregst=-1;

static void updatetoscreen(int cellx,int celly,int visible) {
  int i,no=region[cellx][celly],new=regst(no);
  if(oldregst!=new){
    /*  region has changed status, redraw the entire region */
    for(i=regs[no];i<regs[no+1];i++) touched[regx[i]][regy[i]]=1;
    oldregst=new;
  }
  if(visible) partialredraw();
}

static void applymove(int cellx,int celly,int val) {
  int old=m[cellx][celly],no=region[cellx][celly],border,dir,mask,xd,yd,cx,cy;
  /*  make backup of previous colour for region */
  oldregst=regst(region[cellx][celly]);
  /*  update regblock and regempty */
  if(old==BLOCKED) regblock[no]--;
  else if(old==EMPTY) regempty[no]--;
  if(val==BLOCKED) regblock[no]++;
  else if(val==EMPTY) regempty[no]++;
  /*  apply */
  m[cellx][celly]=val;
  touched[cellx][celly]=1;
  /*  check for adjacent blocked, force redraw for them in order to eventually
      paint them with errorcol  */
  if(old==BLOCKED || val==BLOCKED) for(dir=0;dir<4;dir++) {
    cx=cellx+dx[dir]; cy=celly+dy[dir];
    if(cx<0 || cy<0 || cx>=x || cy>=y || m[cx][cy]!=BLOCKED) continue;
    touched[cx][cy]=1;
  }
  /*  check if there is a strip crossing >=2 boundaries */
  /*  check the following scenarios:
      1.change to empty:
        a.join two strips, each of them can possibly be red
      2.change from empty:
        a.do nothing if st&mask==0 for this cell
        b.split the existing strip into two substrips, check
          the length of each of these and remove red if applicable  */
  /*  try horizontal, then vertical */
  for(dir=0;dir<2;dir++) {
    if(old!=EMPTY && val!=EMPTY) continue;  /*  no change in strips */
    mask=1<<dir;
    if(old==EMPTY && !(st[cellx][celly]&mask)) continue;  /*  case 2a */
    xd=dir==0; yd=dir==1;
    if(val==EMPTY) {
      /*  scenario 1a */
      border=countcrossings(cellx,celly,-xd,-yd)+countcrossings(cellx,celly,xd,yd);
      if(border>1) {
        st[cellx][celly]|=mask;
        updatestdir(cellx-xd,celly-yd,-xd,-yd,0,mask);
        updatestdir(cellx+xd,celly+yd,xd,yd,0,mask);
      }
    } else if(old==EMPTY) {
      st[cellx][celly]&=~mask;
      if(countcrossings(cellx-xd,celly-yd,-xd,-yd)<2) updatestdir(cellx-xd,celly-yd,-xd,-yd,1,mask);
      if(countcrossings(cellx+xd,celly+yd,xd,yd)<2) updatestdir(cellx+xd,celly+yd,xd,yd,1,mask);
    }
  }
}

/*  do move bookkeeping, including putting it on the stack */
static void domove(int cellx,int celly,int val) {
  if(val==m[cellx][celly]) error("logical error, tried to set cell to existing value");
  stackpush(cellx); stackpush(celly); stackpush(m[cellx][celly]);
  applymove(cellx,celly,val);
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

void processmousedown() {
  int cellx,celly,v=controlscheme_heyawake,up=0;
  getcell(event_mousex,event_mousey,&cellx,&celly);
  if(cellx<0 || celly<0 || cellx>=x || celly>=y) return;
  if(event_mousebutton==SDL_BUTTON_LEFT) {
    if(!v) {
      domove(cellx,celly,togglecell(m[cellx][celly])); up=1; normalmove=1; numclicks++;
    } else if(v==1 && m[cellx][celly]!=EMPTY) {
      domove(cellx,celly,EMPTY); up=1; normalmove=1; numclicks++;
    } else if(v==2 && m[cellx][celly]!=BLOCKED) {
      domove(cellx,celly,BLOCKED); up=1; normalmove=1; numclicks++;
    }
  } else if(event_mousebutton==SDL_BUTTON_RIGHT) {
    if(!v && m[cellx][celly]!=UNFILLED) {
      domove(cellx,celly,UNFILLED); up=1; normalmove=1; numclicks++;
    } else if(v==1 && m[cellx][celly]!=BLOCKED) {
      domove(cellx,celly,BLOCKED); up=1; normalmove=1; numclicks++;
    } else if(v==2 && m[cellx][celly]!=EMPTY) {
      domove(cellx,celly,EMPTY); up=1; normalmove=1; numclicks++;
    }
  } else if(event_mousebutton==SDL_BUTTON_MIDDLE) {
    if(v && m[cellx][celly]!=UNFILLED) {
      domove(cellx,celly,UNFILLED); up=1; normalmove=1; numclicks++;
    }
  }
  if(up) updatetoscreen(cellx,celly,1);
}

static void undo(int visible) {
  if(!stackempty()) {
    int val=stackpop(),celly=stackpop(),cellx=stackpop();
    applymove(cellx,celly,val);
    updatetoscreen(cellx,celly,visible);
  }
}

/*  start of hint system! */

static void addmovetoqueue(int cellx,int celly,int val) {
  mq[mqe++]=cellx; mq[mqe++]=celly; mq[mqe++]=val;
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
  if(m[mq[mqs]][mq[mqs+1]]==mq[mqs+2]) {
    mqs+=3;
    if(mqs==MAXMQ) mqs=0;
    goto loop;
  }
  domove(mq[mqs],mq[mqs+1],mq[mqs+2]);
  updatetoscreen(mq[mqs],mq[mqs+1],visible);
  mqs+=3;
  if(mqs==MAXMQ) mqs=0;
  return 1;
}

static void executemovequeue() {
  while(executeonemovefromqueue(1)) if(dummyevent()) drawgrid();
}

static void silentexecutemovequeue() {
  while(executeonemovefromqueue(0));
}

/*  if a region has enough walls, fill the rest with empty */
static int level1fillempty() {
  int i,j,ok=0;
  for(i=0;i<regn;i++) if(regnum[i]>-1 && regblock[i]==regnum[i]) {
    for(j=regs[i];j<regs[i+1];j++) if(m[regx[j]][regy[j]]==UNFILLED) {
      addmovetoqueue(regx[j],regy[j],EMPTY);
      ok=1;
    }
  }
  return ok;
}

/*  if a region has enough empty, fill the rest with wall */
static int level1fillblocked() {
  int i,j,ok=0;
  for(i=0;i<regn;i++) if(regnum[i]>-1 && regnum[i]==regblock[i]+REGSIZE(i)-regblock[i]-regempty[i]) {
    for(j=regs[i];j<regs[i+1];j++) if(m[regx[j]][regy[j]]==UNFILLED) {
      addmovetoqueue(regx[j],regy[j],BLOCKED);
      ok=1;
    }
  }
  return ok;
}

/*  surround walls with empty */
static int level1putemptynearwall() {
  int i,j,k,ok=0,cx,cy;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==BLOCKED) {
    for(k=0;k<4;k++) {
      cx=i+dx[k]; cy=j+dy[k];
      if(cx<0 || cy<0 || cx>=x || cy>=y || m[cx][cy]!=UNFILLED) continue;
      addmovetoqueue(cx,cy,EMPTY);
      ok=1;
    }
  }
  return ok;
}

/*  recognise basic rectangles. easy in principle, lots of hardcoded cases in practice. */
static int level1basicshapes() {
  int i,sx,sy,no,ok=0,cx,cy,j;
  for(i=0;i<regn;i++) if(isrect[i] && (no=regnum[i])>-1) {
    sx=regsizex[i]; sy=regsizey[i];
    cx=regupperx[i]; cy=reguppery[i];
    if(sx==3 && sy==3 && no==5) {
      if(m[cx][cy]==UNFILLED) addmovetoqueue(cx,cy,BLOCKED),ok=1;
      if(m[cx+2][cy]==UNFILLED) addmovetoqueue(cx+2,cy,BLOCKED),ok=1;
      if(m[cx][cy+2]==UNFILLED) addmovetoqueue(cx,cy+2,BLOCKED),ok=1;
      if(m[cx+2][cy+2]==UNFILLED) addmovetoqueue(cx+2,cy+2,BLOCKED),ok=1;
      if(m[cx+1][cy+1]==UNFILLED) addmovetoqueue(cx+1,cy+1,BLOCKED),ok=1;
    }
    if(sx==3 && sy==3 && no==4) {
      if(m[cx][cy+1]==UNFILLED) addmovetoqueue(cx,cy+1,EMPTY),ok=1;
      if(m[cx+2][cy+1]==UNFILLED) addmovetoqueue(cx+2,cy+1,EMPTY),ok=1;
      if(m[cx+1][cy]==UNFILLED) addmovetoqueue(cx+1,cy,EMPTY),ok=1;
      if(m[cx+1][cy+2]==UNFILLED) addmovetoqueue(cx+1,cy+2,EMPTY),ok=1;
    }
    if(sx==2 && sy>2 && no==sy) {
      if(cx) {
        for(j=1;j<sy-1;j++) if(m[cx-1][cy+j]==UNFILLED) addmovetoqueue(cx-1,cy+j,EMPTY),ok=1;
        if(!cy && m[cx-1][cy]==UNFILLED) addmovetoqueue(cx-1,cy,EMPTY),ok=1;
        if(cy+sy==y && m[cx-1][cy+sy-1]==UNFILLED) addmovetoqueue(cx-1,cy+sy-1,EMPTY),ok=1;
      }
      if(cx+2<x) {
        for(j=1;j<sy-1;j++) if(m[cx+2][cy+j]==UNFILLED) addmovetoqueue(cx+2,cy+j,EMPTY),ok=1;
        if(!cy && m[cx+2][cy]==UNFILLED) addmovetoqueue(cx+2,cy,EMPTY),ok=1;
        if(cy+sy==y && m[cx+2][cy+sy-1]==UNFILLED) addmovetoqueue(cx+2,cy+sy-1,EMPTY),ok=1;
      }
    }
    if(sx>2 && sy==2 && no==sx) {
      if(cy) {
        for(j=1;j<sx-1;j++) if(m[cx+j][cy-1]==UNFILLED) addmovetoqueue(cx+j,cy-1,EMPTY),ok=1;
        if(!cx && m[cx][cy-1]==UNFILLED) addmovetoqueue(cx,cy-1,EMPTY),ok=1;
        if(cx+sx==x && m[cx+sx-1][cy-1]==UNFILLED) addmovetoqueue(cx+sx-1,cy-1,EMPTY),ok=1;
      }
      if(cy+2<y) {
        for(j=1;j<sx-1;j++) if(m[cx+j][cy+2]==UNFILLED) addmovetoqueue(cx+j,cy+2,EMPTY),ok=1;
        if(!cx && m[cx][cy+2]==UNFILLED) addmovetoqueue(cx,cy+2,EMPTY),ok=1;
        if(cx+sx==x && m[cx+sx-1][cy+2]==UNFILLED) addmovetoqueue(cx+sx-1,cy+2,EMPTY),ok=1;
      }
    }
    if(sx>2 && (sx&1) && sy==1 && no==(sx+1)/2) {
      for(j=0;j<sx;j+=2) if(m[cx+j][cy]==UNFILLED) addmovetoqueue(cx+j,cy,BLOCKED),ok=1;
    }
    if(sy>2 && (sy&1) && sx==1 && no==(sy+1)/2) {
      for(j=0;j<sy;j+=2) if(m[cx][cy+j]==UNFILLED) addmovetoqueue(cx,cy+j,BLOCKED),ok=1;
    }
    if(sx==2 && sy==2 && no==2) {
      if((!cx && !cy) || (cx==x-2 && cy==y-2)) {
        if(m[cx][cy]==UNFILLED) addmovetoqueue(cx,cy,BLOCKED),ok=1;
        if(m[cx+1][cy+1]==UNFILLED) addmovetoqueue(cx+1,cy+1,BLOCKED),ok=1;
      }
      if((!cx && cy==y-2) || (cx==x-2 && !cy)) {
        if(m[cx][cy+1]==UNFILLED) addmovetoqueue(cx,cy+1,BLOCKED),ok=1;
        if(m[cx+1][cy]==UNFILLED) addmovetoqueue(cx+1,cy,BLOCKED),ok=1;
      }
      if(!cy) {             /*  at top */
        if(cx && m[cx-1][cy]==UNFILLED) addmovetoqueue(cx-1,cy,EMPTY),ok=1;
        if(cx<x-2 && m[cx+2][cy]==UNFILLED) addmovetoqueue(cx+2,cy,EMPTY),ok=1;
      } else if(cy==y-2) {  /*  at bottom */
        if(cx && m[cx-1][cy+1]==UNFILLED) addmovetoqueue(cx-1,cy+1,EMPTY),ok=1;
        if(cx<x-2 && m[cx+2][cy+1]==UNFILLED) addmovetoqueue(cx+2,cy+1,EMPTY),ok=1;
      } else if(!cx) {      /*  at left */
        if(cy && m[cx][cy-1]==UNFILLED) addmovetoqueue(cx,cy-1,EMPTY),ok=1;
        if(cy<y-2 && m[cx][cy+2]==UNFILLED) addmovetoqueue(cx,cy+2,EMPTY),ok=1;
      } else if(cx==x-2) {  /*  at right */
        if(cy && m[cx+1][cy-1]==UNFILLED) addmovetoqueue(cx+1,cy-1,EMPTY),ok=1;
        if(cy<y-2 && m[cx+1][cy+2]==UNFILLED) addmovetoqueue(cx+1,cy+2,EMPTY),ok=1;
      }
    }
    if(sx==3 && sy==2 && no==3) {
      if(!cy) {
        if(m[cx][cy+1]==UNFILLED) addmovetoqueue(cx,cy+1,BLOCKED),ok=1;
        if(m[cx+1][cy]==UNFILLED) addmovetoqueue(cx+1,cy,BLOCKED),ok=1;
        if(m[cx+2][cy+1]==UNFILLED) addmovetoqueue(cx+2,cy+1,BLOCKED),ok=1;
      } else if(cy==y-2) {
        if(m[cx][cy]==UNFILLED) addmovetoqueue(cx,cy,BLOCKED),ok=1;
        if(m[cx+1][cy+1]==UNFILLED) addmovetoqueue(cx+1,cy+1,BLOCKED),ok=1;
        if(m[cx+2][cy]==UNFILLED) addmovetoqueue(cx+2,cy,BLOCKED),ok=1;
      }
    }
    if(sx==2 && sy==3 && no==3) {
      if(!cx) {
        if(m[cx+1][cy]==UNFILLED) addmovetoqueue(cx+1,cy,BLOCKED),ok=1;
        if(m[cx][cy+1]==UNFILLED) addmovetoqueue(cx,cy+1,BLOCKED),ok=1;
        if(m[cx+1][cy+2]==UNFILLED) addmovetoqueue(cx+1,cy+2,BLOCKED),ok=1;
      } else if(cx==x-2) {
        if(m[cx][cy]==UNFILLED) addmovetoqueue(cx,cy,BLOCKED),ok=1;
        if(m[cx+1][cy+1]==UNFILLED) addmovetoqueue(cx+1,cy+1,BLOCKED),ok=1;
        if(m[cx][cy+2]==UNFILLED) addmovetoqueue(cx,cy+2,BLOCKED),ok=1;
      }
    }
  }
  return ok;
}

static int level1hint() {
  if(level1fillempty()) return 1;
  if(level1fillblocked()) return 1;
  if(level1putemptynearwall()) return 1;
  if(level1basicshapes()) return 1;
  return 0;
}

/*  check if filling a cell with empty causes long strip */
static int level2strip() {
  int i,j,border;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    /*  sneaky modification */
    m[i][j]=EMPTY;
    /*  horizontal */
    border=countcrossings(i,j,-1,0)+countcrossings(i,j,1,0);
    if(border>1) {
      m[i][j]=UNFILLED;
      addmovetoqueue(i,j,BLOCKED);
      return 1;
    }
    /*  vertical */
    border=countcrossings(i,j,0,-1)+countcrossings(i,j,0,1);
    if(border>1) {
      m[i][j]=UNFILLED;
      addmovetoqueue(i,j,BLOCKED);
      return 1;
    }
    m[i][j]=UNFILLED;
  }
  return 0;
}

/*  check if a cell is enclosed with 3 walls */
static int level2almostenclosed() {
  int i,j,k,count,tot,cx,cy,x2=0,y2=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==EMPTY) {
    count=tot=0;
    for(k=0;k<4;k++) {
      cx=i+dx[k]; cy=j+dy[k];
      if(cx<0 || cy<0 || cx>=x || cy>=y) continue;
      tot++;
      if(m[cx][cy]==BLOCKED) count++;
      else x2=cx,y2=cy;
    }
    if(count==tot-1 && m[x2][y2]!=EMPTY) {
      addmovetoqueue(x2,y2,EMPTY);
      return 1;
    }
  }
  return 0;
}

static int level2hint() {
  if(level2strip()) return 1;
  if(level2almostenclosed()) return 1;
  return 0;
}

/*  we need a separate array to avoid reuse conflicts */
#define NOINTERSECT 100
#define NOVALUE     101
static int btrres[MAXS][MAXS];

/*  check if blank is surrounded by walls */
static int surrounded1x1(int cx,int cy) {
  int d,x2,y2;
  /*  if cx,cy is outside of the maze, it isn't considered to be surrounded */
  if(cx<0 || cy<0 || cx>=x || cy>=y) return 0;
  for(d=0;d<4;d++) {
    x2=cx+dx[d]; y2=cy+dy[d];
    if(x2<0 || y2<0 || x2>=x || y2>=y) continue;
    if(m[x2][y2]!=BLOCKED) return 0;
  }
  return 1;
}

/*  during backtracking, only check for neighbouring walls and blocked 1x1 cells */
/*  TODO speed up backtracking: calculate real left values from the used cells */
static void level3btr(int regno,int at,int left,void (*callback)(int)) {
  int cx=regx[regs[regno]+at],cy=regy[regs[regno]+at],count,i,val;
  if(at==REGSIZE(regno)) {
    if(!left) (*callback)(regno);
    return;
  }
  if(left>REGSIZE(regno)-at) return;
  if(m[cx][cy]!=UNFILLED) { level3btr(regno,at+1,left,callback); return; }
  /*  check for neighbouring wall */
  if(left && !hasadjacentblocked(cx,cy)) {
    m[cx][cy]=BLOCKED;
    /*  check if we are next to a nearby surrounded 1x1 */
    if(!surrounded1x1(cx-1,cy) && !surrounded1x1(cx+1,cy) && !surrounded1x1(cx,cy-1) && !surrounded1x1(cx,cy+1))
      level3btr(regno,at+1,left-1,callback);
  }
  for(count=i=0;i<4;i++) {
    val=readcell(cx+dx[i],cy+dy[i]);
    if(val==OUTOFBOUNDS || val==BLOCKED) count++;
  }
  if(count<4) {
    m[cx][cy]=EMPTY;
    level3btr(regno,at+1,left,callback);
  }
  m[cx][cy]=UNFILLED;
}

static void level3combinationprocess(int regno) {
  int i,cx,cy;
  for(i=regs[regno];i<regs[regno+1];i++) {
    cx=regx[i]; cy=regy[i];
    if(m[cx][cy]!=btrres[cx][cy]) btrres[cx][cy]=(btrres[cx][cy]==UNFILLED)?m[cx][cy]:NOINTERSECT;
  }
  return;
}

static int level3combination(int comb) {
  int i,n,c,j,ok=0;
  for(i=0;i<regn;i++) if(regnum[i]>-1) {
    n=REGSIZE(i)-regblock[i]-regempty[i];
    if(n>=MAXP) continue;
    if(n<=0) continue;
    c=regnum[i]-regblock[i];
    if(pascal[n][c]>comb) continue;
    /*  proceed, since we have an overcomable number of combinations */
    /*  prepare data structure */
    for(j=regs[i];j<regs[i+1];j++) btrres[regx[j]][regy[j]]=UNFILLED;
    /*  search! */
    level3btr(i,0,regnum[i]-regblock[i],level3combinationprocess);
    /*  check the resulting intersection: apply what we got */
    for(j=regs[i];j<regs[i+1];j++) if(btrres[regx[j]][regy[j]]!=NOINTERSECT && btrres[regx[j]][regy[j]]!=UNFILLED && m[regx[j]][regy[j]]!=btrres[regx[j]][regy[j]]) {
      addmovetoqueue(regx[j],regy[j],btrres[regx[j]][regy[j]]);
      ok=1;
    }
    if(ok) return 1;
  }
  return 0;
}

#define MAXLEVEL3COMB 200
static int level3hint() {
  if(level3combination(MAXLEVEL3COMB)) return 1;
  return 0;
}

static int level4disconnect() {
  int i,j,k,l=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    /*  plug it in */
    m[i][j]=BLOCKED;
    /*  check if the entire region is connected,
        start searching from arbitrary empty cell */
    for(k=0;k<x;k++) for(l=0;l<y;l++) if(m[k][l]<BLOCKED) goto found;
  found:
    genericbfs(k,l,0);
    /*  check if unvisited non-wall exists */
    for(k=0;k<x;k++) for(l=0;l<y;l++) if(m[k][l]<BLOCKED && !visit[k][l]) {
      m[i][j]=UNFILLED;
      cleanupbfs();
      addmovetoqueue(i,j,EMPTY);
      return 1;
    }
    m[i][j]=UNFILLED;
    cleanupbfs();
  }
  return 0;
}

static int level4hint() {
  if(level4disconnect()) return 1;
  return 0;
}

static int lev5alt1[MAXS][MAXS];
static int lev5alt2[MAXS][MAXS];

/*  copy board from a to b */
static void copyboard(int a[MAXS][MAXS],int b[MAXS][MAXS]) {
  int i,j;
  for(i=0;i<x;i++) for(j=0;j<y;j++) b[i][j]=a[i][j];
}

static int dogreedy(int lev) {
  int r;
  while(1) {
    r=verifyboard();
    if(r) return r;
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
  static int i=0,j=0;
  int z=x*y,r,oldsp=getstackpos(),k,l;
	if(i>=x) i=0;
	if(j>=y) j=0;
  while(z--) {
    if(m[i][j]==UNFILLED) {
      /*  assume blocked */
      domove(i,j,BLOCKED);
      updatetoscreen(i,j,0);
      r=dogreedy(lev);
      if(r>0) {
        /*  solved! */
        while(getstackpos()>oldsp) undo(0);
        addmovetoqueue(i,j,BLOCKED);
        return 1;
      } else if(r<0) {
        /*  contradiction */
        while(getstackpos()>oldsp) undo(0);
        addmovetoqueue(i,j,EMPTY);
        return 1;
      }
      copyboard(m,lev5alt1);
      while(getstackpos()>oldsp) undo(0);
      /*  assume empty */
      domove(i,j,EMPTY);
      updatetoscreen(i,j,0);
      r=dogreedy(lev);
      if(r>0) {
        /*  solved! */
        while(getstackpos()>oldsp) undo(0);
        addmovetoqueue(i,j,EMPTY);
        return 1;
      } else if(r<0) {
        /*  contradiction! */
        while(getstackpos()>oldsp) undo(0);
        addmovetoqueue(i,j,BLOCKED);
        return 1;
      }
      copyboard(m,lev5alt2);
      while(getstackpos()>oldsp) undo(0);
      for(r=k=0;k<x;k++) for(l=0;l<y;l++) if(lev5alt1[k][l]!=UNFILLED && lev5alt1[k][l]==lev5alt2[k][l] && m[k][l]==UNFILLED)
        addmovetoqueue(k,l,lev5alt1[k][l]),r=1;
      if(r) return 1;
    }
    j++;
    if(j==y) {
      j=0; i++;
      if(i==x) i=0;
    }
  }
  return 0;
}

static int btrres5[MAXS][MAXS];

static void level5combinationprocess(int regno) {
  int i,j,oldsp=getstackpos(),r;
  /*  from here, do greedy */
  r=dogreedy(4);
  if(r<0) {
    while(getstackpos()>oldsp) undo(0);
    return;
  }
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    if(btrres5[i][j]==NOVALUE) btrres5[i][j]=m[i][j];
    else if(btrres5[i][j]!=NOINTERSECT && m[i][j]!=btrres5[i][j]) btrres5[i][j]=NOINTERSECT;
  }
  while(getstackpos()>oldsp) undo(0);
  return;
}

/* i don't remember why i ended up using level3btr instead. it looks inferior,
   since it plots cells directly instead of using domove. anyway, this
   function is never called from the outside */
// commented out since it gave a warning
/*
static void level5btr(int regno,int at,int left,void (*callback)(int)) {
	int cx=regx[regs[regno]+at],cy=regy[regs[regno]+at],count,i,val;
	if(at==REGSIZE(regno)) {
		if(!left) (*callback)(regno);
		return;
	}
	if(left>REGSIZE(regno)-at) return;
	if(m[cx][cy]!=UNFILLED) { level5btr(regno,at+1,left,callback); return; }
	// blocked
	if(left && !hasadjacentblocked(cx,cy)) {
		domove(cx,cy,BLOCKED);
		if(!surrounded1x1(cx-1,cy) && !surrounded1x1(cx+1,cy) && !surrounded1x1(cx,cy-1) && !surrounded1x1(cx,cy+1))
			level5btr(regno,at+1,left-1,callback);
		undo(0);
	}
	for(count=i=0;i<4;i++) {
		val=readcell(cx+dx[i],cy+dy[i]);
		if(val==OUTOFBOUNDS || val==BLOCKED) count++;
	}
	if(count<4) {
		domove(cx,cy,EMPTY);
		level5btr(regno,at+1,left,callback);
		undo(0);
	}
}
*/

static int level5combination(int comb) {
  int i,n,c,ok=0,l,k;
  for(i=0;i<regn;i++) if(regnum[i]>-1) {
    /* don't search region if it is complete */
    n=REGSIZE(i)-regblock[i]-regempty[i];
    if(n<1) continue;
    if(n>=MAXP) continue;
    c=regnum[i]-regblock[i];
    if(pascal[n][c]>comb) continue;
    /*  proceed, since we have an overcomable number of combinations */
    /*  prepare data structure */
    for(k=0;k<x;k++) for(l=0;l<y;l++) btrres5[k][l]=NOVALUE;
    /*  search! */
    level3btr(i,0,regnum[i]-regblock[i],level5combinationprocess);
    /*  check the resulting intersection: apply what we got */
    for(k=0;k<x;k++) for(l=0;l<y;l++) if(btrres5[k][l]!=NOINTERSECT && btrres5[k][l]!=UNFILLED && m[k][l]==UNFILLED) {
      ok=1;
      addmovetoqueue(k,l,btrres5[k][l]);
    }
    if(ok) return 1;
  }
  return 0;
}

#define MAXLEVEL5COMB 500
static int level5hint() {
  if(level5contradiction(4)) return 1;
  if(level5combination(MAXLEVEL5COMB)) return 1;
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

#define COLSIZE 6
static Uint32 colarray[]={
  0xFFDBFF, 0xFFFF00, 0xFFFFAA, 0xB6FF00, 0x92FFAA, 0xFFB600};

static int forcefullredraw;

static void drawverify() {
  int i,j,col=0,k,qs2;
  if(forcefullredraw) drawgrid();
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && m[i][j]==EMPTY) {
    qs2=qs;
    genericbfs(i,j,1);
    for(k=qs2;k<qe;k+=2) updatecell(q[k],q[k+1],colarray[col]);
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
  for(i=0;i<x;i++) for(j=0;j<y;j++) updatecell(i,j,-1);
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
      res=verifyboard();
      if(res<0) messagebox(1,"Sorry, the solver did something wrong.\n");
      else if(res<1) messagebox(1,"Sorry, no more moves found.");
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

void heyawake(char *path,int solve) {
  int event,oldthick=thick;
  autosolve=solve;
  if(heyawakethick>-1) thick=heyawakethick;
  loadpuzzle(path);
  initbfs();
  genpascal();
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
