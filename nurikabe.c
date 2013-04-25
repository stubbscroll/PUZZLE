#include <stdio.h>
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

static int dummy;               /*  dummy variable */

/*  the actual content of a cell resides within [thick,width)x[thick,height).
    hence, the entire draw ares is [0,width*x+thick)x[0,height*y+thick),
    before translation. */
/*  the actual variables width,height are stored in graphics.c */

/*  NB, we must have UNFILLED<BLOCKED<EMPTY<1 (numbered cell) */
#define UNFILLED    -100
#define BLOCKED     -1
#define EMPTY       0

/*  internal format:
    -100: unfilled cell
    -1: blocked cell (water, wall)
    0:  empty cell (island, room)
    1-  numbered cell (part of island)
*/
static int m[MAXS][MAXS];
/*  additional info:
    0:  nothing special (island too small)
    1:  island is complete, but need to be closed
    2:  island is complete
    3:  island is too large, or island is closed with wrong amount,
        or island has 0 or 2 numbered cells
*/
#define ISLAND_TOOSMALL 0
#define ISLAND_TOCLOSE  1
#define ISLAND_GOOD     2
#define ISLAND_TOOLARGE 3
static int st[MAXS][MAXS];
static char touched[MAXS][MAXS];  /*  1 if cell is changed and need to be redrawn */

static int totalnum;              /*  sum of numbered cells = number of blanks */
/*  also: x*y-totalnum: required number of walls */

/*  move queue for hint system */
#define MAXMQ MAXS*MAXS*3
static int mq[MAXMQ];
static int mqs,mqe;

static int autosolve;

static int convchar(char *s,int *i) {
  char c=s[(*i)++];
  int v;
  if(!c) error("string in level definition ended prematurely.");
  else if(c=='.') return UNFILLED;
  else if(c>='1' && c<='9') return c-48;
  else if(c>='a' && c<='z') return c-'a'+10;
  else if(c>='A' && c<='Z') return c-'A'+36;
  else if(c=='{') {
    v=0;
    while(s[*i] && s[*i]!='}') v=v*10+s[(*i)++]-48;
    if(s[*i]=='}') (*i)++;
    return v;
  } else if(c=='#') return BLOCKED;
  else if(c=='_') return EMPTY;
  /*  ^ super sneaky! allow walls and empty in level definition for
      easier debugging. naturally, a proper level will never have
      these symbols */
  error("invalid character %c in level definition.",c);
  return 0;
}

/*  use this as a model for loadpuzzle for other puzzles */
/*  TODO abstrahate this? */
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
      for(i=j=0;j<x;j++) m[j][ln]=convchar(s,&i);
      ln++;
    }
  } else if(!gameinfo[0]) strcpy(gameinfo,s+2);
  fclose(f);
  /*  set top left of grid area in window */
  startx=10,starty=(int)(font->height*2.5);
  totalnum=mqs=mqe=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    st[i][j]=touched[i][j]=0;
    if(m[i][j]>0) totalnum+=m[i][j];
  }
}

static void updatecell(int u,int v) {
  /*  undetermined - white */
  if(m[u][v]==UNFILLED) drawsolidcell32(u,v,unfilledcol);
  /*  water, wall */
  else if(m[u][v]==BLOCKED) drawsolidcell32(u,v,filledcol);
  else if(m[u][v]==EMPTY) {
    /*  unnumbered island, room */
    if(st[u][v]==ISLAND_TOOSMALL) drawsolidcell32(u,v,blankcol);
    else if(st[u][v]==ISLAND_TOCLOSE) drawsolidcell32(u,v,almostokcol);
    else if(st[u][v]==ISLAND_GOOD) drawsolidcell32(u,v,okcol);
    else if(st[u][v]==ISLAND_TOOLARGE) drawsolidcell32(u,v,errorcol);
  } else if(m[u][v]>0) {
    /*  numbered island, room */
    if(st[u][v]==ISLAND_TOOSMALL) drawnumbercell32(u,v,m[u][v],BLACK32,BLACK32,blankcol);
    else if(st[u][v]==ISLAND_TOCLOSE) drawnumbercell32(u,v,m[u][v],BLACK32,BLACK32,almostokcol);
    else if(st[u][v]==ISLAND_GOOD) drawnumbercell32(u,v,m[u][v],BLACK32,BLACK32,okcol);
    else if(st[u][v]==ISLAND_TOOLARGE) drawnumbercell32(u,v,m[u][v],BLACK32,BLACK32,errorcol);
  }
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

static int togglecell(int val) {
  switch(val) {
  case UNFILLED:
  case EMPTY:
    return BLOCKED;
  case BLOCKED:
    return EMPTY;
  default:
    error("board hath illegal values.");
    return 0;
  }
}

/*  TODO oh noes, here we loop through the entire board.
    if this ever turns out to be a performance problem, make
    a list of touched cells instead */
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

/*  here starts routines to recalculate the fancy background colours for
    the cells. for each change, examine the neighbourhood of the change
    and determine if regions contains enough cells, too many cells, or
    too many numbered cells. */

/*  nice general purpose bfs variables (for completeness check and hint system) */
static uchar visit[MAXS][MAXS];
static int qs,qe,q[MAXS*MAXS*2];
static int fromx[MAXS][MAXS],fromy[MAXS][MAXS];
/*  for type 4 bfs: store the position of the numbered cell */
static int foundx,foundy;

/*  only needs to be called once */
static void initcompleteness() {
  memset(visit,0,sizeof(visit));
  qs=qe=0;
}

/*  type:
    0: search for empty, numbered
    1: search for blocked
    2: search for empty, numbered, unfilled
    3: search for blocked, unfilled
    4: search for empty, numbered, unfilled + update fromx[][],fromy[][] and
       set foundx,foundy to the coordinate of the numbered cell
    return: tot: number of cells examined
            num: numbered cell (-1:more than one, >0: the number, 0: no numbered cell
            islands: number of island cells found
*/
static void genericbfs(int sx,int sy,int type,int *tot,int *num,int *islands) {
  int cx,cy,x2,y2,i;
  *tot=*num=*islands=0;
  if(visit[sx][sy]) return;
  if(type==0 && m[sx][sy]<EMPTY) return;
  if(type==1 && m[sx][sy]!=BLOCKED) return;
  if(type==2 && m[sx][sy]<EMPTY && m[sx][sy]!=UNFILLED) return;
  if(type==3 && m[sx][sy]!=BLOCKED && m[sx][sy]!=UNFILLED) return;
  if(type==4 && m[sx][sy]<EMPTY && m[sx][sy]!=UNFILLED) return;
  if(type==4) foundx=foundy=fromx[sx][sy]=fromy[sx][sy]=-1;
  q[qe++]=sx,q[qe++]=sy;
  visit[sx][sy]=1;
  *tot=1;
  /*  WARNING, recently fixed this */
  if(m[sx][sy]>=EMPTY) (*islands)++;
  *num=m[sx][sy]>0?m[sx][sy]:0;
  while(qs<qe) {
    cx=q[qs++],cy=q[qs++];
    for(i=0;i<4;i++) {
      x2=cx+dx[i],y2=cy+dy[i];
      if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2]) continue;
      if(type==0 && m[x2][y2]<EMPTY) continue;
      if(type==1 && m[x2][y2]!=BLOCKED) continue;
      if(type==2 && m[x2][y2]<EMPTY && m[x2][y2]!=UNFILLED) continue;
      if(type==3 && m[x2][y2]!=BLOCKED && m[x2][y2]!=UNFILLED) continue;
      if(type==4 && m[x2][y2]<EMPTY && m[x2][y2]!=UNFILLED) continue;
      if(type==4 && m[x2][y2]>0 && foundx<0) foundx=x2,foundy=y2;
      /*  only update from in type 4 */
      if(type==4) {
        fromx[x2][y2]=cx;
        fromy[x2][y2]=cy;
      }
      (*tot)++;
      if(m[x2][y2]>=EMPTY) (*islands)++;
      if(m[x2][y2]>0) {
        if(!*num) *num=m[x2][y2];
        else *num=-1;
      }
      q[qe++]=x2,q[qe++]=y2;
      visit[x2][y2]=1;
    }
  }
}

/*  check region completeness from cellx,celly */
/*  WARNING, call cleanupbfs() after this */
static void recalccompleteness(int cellx,int celly,int visible) {
  int i,j,x2,y2,qs2=qe,ss=0,closed=1;
  int tot=1;    /*  # connected blank cells */
  int num=0;    /*  0: no number in region, >0: number encountered in region,
                    -1: more than one numbered cell in region */
  if(cellx<0 || celly<0 || cellx>=x || celly>=y || m[cellx][celly]==BLOCKED
    || m[cellx][celly]==UNFILLED || visit[cellx][celly]) return;
  genericbfs(cellx,celly,0,&tot,&num,&dummy);
  /*  check if region is enclosed */
  for(i=qs2;i<qe;i+=2) for(j=0;j<4;j++) {
    x2=q[i]+dx[j],y2=q[i+1]+dy[j];
    if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]==UNFILLED) {
      closed=0;
      goto found;
    }
  }
found:
  /*  region is found, update colours */
  if(closed) {
    if(num<1 || num!=tot) ss=ISLAND_TOOLARGE;
    else ss=ISLAND_GOOD;
  } else {
    if(num<0 || (num>0 && tot>num)) ss=ISLAND_TOOLARGE;
    else if(num>0 && tot==num) ss=ISLAND_TOCLOSE;
    else ss=ISLAND_TOOSMALL;
  }
  for(i=qs2;i<qe;i+=2) {
    x2=q[i],y2=q[i+1];
    if(st[x2][y2]!=ss) st[x2][y2]=ss,touched[x2][y2]=visible;
  }
}

/*  clean up the data structures, clear visited and reset queue pos
    (must be done after a series of bfs-es */
static void cleanupbfs() {
  while(qe) visit[q[qe-2]][q[qe-1]]=0,qe-=2;
  qs=0;
}

/*  do move bookkeeping, including putting it on the stack */
static void domove(int cellx,int celly,int val) {
  if(val==m[cellx][celly]) error("logical error, tried to set cell to existing value");
  stackpush(cellx); stackpush(celly); stackpush(m[cellx][celly]);
  m[cellx][celly]=val;
  touched[cellx][celly]=1;
}

static void updatetoscreen(int cellx,int celly,int visible) {
  int i;
  for(i=0;i<5;i++) recalccompleteness(cellx+dx[i],celly+dy[i],visible);
  cleanupbfs();
  if(visible) partialredraw();
}

/*  TODO maybe find a better way to organize the following code */
/*  change board according to mouse click */
static void processmousedown() {
  int cellx,celly,v=controlscheme_nurikabe,up=0;
  if(event_mousebutton==SDL_BUTTON_LEFT) {
    getcell(event_mousex,event_mousey,&cellx,&celly);
    if(cellx<0 || celly<0 || cellx>=x || celly>=y || m[cellx][celly]>0) return;
    if(!v) {
      domove(cellx,celly,togglecell(m[cellx][celly])); up=1; normalmove=1; numclicks++;
    } else if(v==1 && m[cellx][celly]!=EMPTY) {
      domove(cellx,celly,EMPTY); up=1; normalmove=1; numclicks++;
    } else if(v==2 && m[cellx][celly]!=BLOCKED) {
      domove(cellx,celly,BLOCKED); up=1; normalmove=1; numclicks++;
    }
  } else if(event_mousebutton==SDL_BUTTON_RIGHT) {
    getcell(event_mousex,event_mousey,&cellx,&celly);
    if(cellx<0 || celly<0 || cellx>=x || celly>=y || m[cellx][celly]>0) return;
    if(!v && m[cellx][celly]!=UNFILLED) {
      domove(cellx,celly,UNFILLED); up=1; normalmove=1; numclicks++;
    } else if(v==1 && m[cellx][celly]!=BLOCKED) {
      domove(cellx,celly,BLOCKED); up=1; normalmove=1; numclicks++;
    } else if(v==2 && m[cellx][celly]!=EMPTY) {
      domove(cellx,celly,EMPTY); up=1; normalmove=1; numclicks++;
    }
  } else if(event_mousebutton==SDL_BUTTON_MIDDLE) {
    getcell(event_mousex,event_mousey,&cellx,&celly);
    if(cellx<0 || celly<0 || cellx>=x || celly>=y || m[cellx][celly]>0) return;
    if(v && m[cellx][celly]!=UNFILLED) {
      domove(cellx,celly,UNFILLED); up=1; normalmove=1; numclicks++;
    }
  }
  if(up) updatetoscreen(cellx,celly,1);
}

static void undo(int vis) {
  if(!stackempty()) {
    int val=stackpop(),celly=stackpop(),cellx=stackpop();
    m[cellx][celly]=val;
    touched[cellx][celly]=1;
    updatetoscreen(cellx,celly,vis);
  }
}

/*  hint system! */
/*  warning, this is not very efficient */

static void addmovetoqueue(int cellx,int celly,int val) {
  mq[mqe++]=cellx; mq[mqe++]=celly; mq[mqe++]=val;
  if(mqe==MAXMQ) mqe=0;
}

static int movequeueisempty() {
  return mqs==mqe;
}

/*  return:
    0: no moves in queue
    1: move successfully executed */
static int executeonemovefromqueue(int visible) {
loop:
  if(movequeueisempty()) return 0;
  /*  the hint system can produce some moves twice, don't redo moves */
  if(m[mq[mqs]][mq[mqs+1]]==mq[mqs+2]) {
    mqs+=3; if(mqs==MAXMQ) mqs=0;
    goto loop;
  }
  domove(mq[mqs],mq[mqs+1],mq[mqs+2]);
  updatetoscreen(mq[mqs],mq[mqs+1],visible);
  mqs+=3; if(mqs==MAXMQ) mqs=0;
  return 1;
}

static void executemovequeue() {
  /*  update screen and allow resizing/quitting while program is "blocked" */
  while(executeonemovefromqueue(1)) if(dummyevent()) drawgrid();
}

static void silentexecutemovequeue() {
  while(executeonemovefromqueue(0));
}

/*  another helper array for hint */
/*  rather arbitrary limit on the number of islands */
#define MAXISLAND 1000
static int islandmap[MAXS][MAXS];
static int islandsize[MAXISLAND],islandid,islandidnum;
static int islandnum[MAXISLAND];

/*  reachability array used by some subroutines */
static uchar reach[MAXS][MAXS];

/*  helper routine: mark all islands with id */
/*  islandmap[x][y] is set to -1 if no island, otherwise an id
    islandsize[id] indicates the size of island number id
    islandnum[id] indicates the numbered cell within island number id,
      0 if no number
    islandid is the number of ids
    islandidnum is the number of islands with numbered cells
    the id for an island is >=0, <islandidnum if it contains a number
    it is >=islandidnum if it doesn't */
/*  return 0 if the number of islands was exceeded */
static int markallislandswithid() {
  int i,j,k,qs2,num,tot;
  for(islandid=i=0;i<x;i++) for(j=0;j<y;j++) islandmap[i][j]=-1;
  /*  mark all numbered islands */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(islandmap[i][j]==-1 && m[i][j]>0) {
    if(islandid==MAXISLAND) return 0;
    qs2=qe;
    genericbfs(i,j,0,&tot,&num,&dummy);
    for(k=qs2;k<qe;k+=2) islandmap[q[k]][q[k+1]]=islandid;
    islandsize[islandid]=tot;
    islandnum[islandid++]=num;
  }
  islandidnum=islandid;
  /*  mark all nonnumbered islands */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(islandmap[i][j]<0 && m[i][j]==EMPTY) {
    if(islandid==MAXISLAND) return 0;
    qs2=qe;
    genericbfs(i,j,0,&tot,&num,&dummy);
    for(k=qs2;k<qe;k+=2) islandmap[q[k]][q[k+1]]=islandid;
    islandsize[islandid]=tot;
    islandnum[islandid++]=0;
  }
  cleanupbfs();
  return 1;
}

/*  return 1 if solved, 0 if unsolved, -1 if error somewhere */
/*  assume that st is correctly filled in with island sizes! */
/*  TODO rewrite this so it doesn't depend on st[][]? should then be
    easier to use as subroutine in solver */
static int verifyboard() {
  int i,j,tot,num;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>=EMPTY && st[i][j]==ISLAND_TOOLARGE) return -1;
  for(i=0;i<x-1;i++) for(j=0;j<y-1;j++) if(m[i][j]==BLOCKED && m[i+1][j]==BLOCKED && m[i][j+1]==BLOCKED && m[i+1][j+1]==BLOCKED) return -1;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) return 0;
  /*  ok, no island errors and everything is filled in.
      check if blocked is connected */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==BLOCKED) {
    genericbfs(i,j,1,&tot,&num,&dummy);
    cleanupbfs();
    if(tot!=x*y-totalnum) return -1;
  }
  return 1;
}

/*  deeper verify:
    - check if walls cannot be connected
    - check if there are islands that cannot grow to their full size
    - check if there exists unnumbered islands which cannot be
      connected to any existing numbered island. this heuristic
      will improve contradiction <- important
    TODO check if there exists an enclosure with m islands, n squares,
         sum of islands k such that k-m-1>n
*/
static int deepverifyboard() {
  static uchar reachisland[MAXISLAND];
  int r=verifyboard(),i,j,left,cx,cy,k,x2,y2,l,x3,y3,tot,num,id,dummy,qe2,steps;
  if(r) return r;
  /*  there are unfilled cells. check if walls can be connected */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==BLOCKED) {
    genericbfs(i,j,3,&tot,&num,&dummy);
    for(k=0;k<x;k++) for(l=0;l<y;l++) if(m[k][l]==BLOCKED && !visit[k][l]) {
      cleanupbfs();
      return -1;
    }
    cleanupbfs();
    goto breakout;
  }
breakout:
  if(!markallislandswithid()) return 0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>EMPTY) {
    left=m[i][j]-1;
    q[qe++]=i; q[qe++]=j;
    visit[i][j]=1;
    while(qs<qe) {
      cx=q[qs++]; cy=q[qs++];
      for(k=0;k<4;k++) {
        x2=cx+dx[k],y2=cy+dy[k];
        if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2] || m[x2][y2]<UNFILLED) continue;
        /*  check if cell is adjacent to another island */
        for(l=0;l<4;l++) {
          x3=x2+dx[l],y3=y2+dy[l];
          if(x3<0 || y3<0 || x3>=x || y3>=y || visit[x3][y3] || m[x3][y3]<EMPTY) continue;
          if(islandmap[x3][y3]>-1 && islandmap[x2][y2]!=islandmap[x3][y3] && islandmap[x3][y3]<islandidnum) goto nomove;
        }
        if(left>0) q[qe++]=x2,q[qe++]=y2,left--,visit[x2][y2]=1;
      nomove:;
      }
    }
    cleanupbfs();
    if(left) return -1;
  }
  /*  check if there are legally unreachable unnumbered islands */
  for(i=islandidnum;i<islandid;i++) reachisland[i]=0;
  /*  from each numbered island, search */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>0) {
    id=islandmap[i][j];
    if(id<0) error("error in deepverify, at %d %d, island without id\n",i,j);
    genericbfs(i,j,0,&tot,&num,&dummy);
    if(num<0 || tot>num) { logprintf("quitteth\n"); cleanupbfs(); return -1; }
    qs=0; qe2=qe;
    left=num-tot-1;
    steps=0;
    while(left-->0) {
      while(qs<qe) {
        cx=q[qs++]; cy=q[qs++];
        for(k=0;k<4;k++) {
          x2=cx+dx[k],y2=cy+dy[k];
          if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2] || m[x2][y2]==BLOCKED) continue;
          /*  check if x2,y2 is adjacent to another numbered island */
          for(l=0;l<4;l++) {
            x3=x2+dx[l],y3=y2+dy[l];
            if(x3<0 || y3<0 || x3>=x || y3>=y || (x2==x3 && y2==y3) || m[x3][y3]<EMPTY) continue;
            if(islandmap[x3][y3]<islandidnum && islandmap[x3][y3]!=id) goto next;
          }
          /*  check if x2,y2 is adjacent to unnumbered islands: add them */
          for(l=0;l<4;l++) {
            x3=x2+dx[l],y3=y2+dy[l];
            if(x3<0 || y3<0 || x3>=x || y3>=y || (x2==x3 && y2==y3) || m[x3][y3]<EMPTY) continue;
            if(islandmap[x3][y3]!=id && tot+steps+1+islandsize[islandmap[x3][y3]]<=num) reachisland[islandmap[x3][y3]]=1;
          }
          visit[x2][y2]=1;
          q[qe2++]=x2; q[qe2++]=y2;
        next:;
        }
      }
      qe=qe2;
      steps++;
    }
    cleanupbfs();
  }
  for(i=islandidnum;i<islandid;i++) if(!reachisland[i]) return -1;
  return 0;
}

static int level1surround1() {
  int i,j,k,x2,y2,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==1) {
    for(k=0;k<4;k++) {
      x2=i+dx[k],y2=j+dy[k];
      if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]!=UNFILLED) continue;
      addmovetoqueue(x2,y2,BLOCKED);
      ok=1;
    }
  }
  return ok;
}

static int level12x2() {
  int i,j,k,l,count,x2=0,y2=0,ok=0;
  for(i=0;i<x-1;i++) for(j=0;j<y-1;j++) {
    for(count=k=0;k<2;k++) for(l=0;l<2;l++) if(m[i+k][j+l]==BLOCKED) count++;
    else x2=i+k,y2=j+l;
    if(count==3 && m[x2][y2]==UNFILLED) addmovetoqueue(x2,y2,EMPTY),ok=1;
  }
  return ok;
}

/*  find unfilled squares having two numbers as neighbours */
static int level1twonumberedneighbours() {
  int i,j,k,x2,y2,c,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    for(c=k=0;k<4;k++) {
      x2=i+dx[k],y2=j+dy[k];
      if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]>0) c++;
    }
    if(c>1) addmovetoqueue(i,j,BLOCKED),ok=1;
  }
  return ok;
}

static int level1enclosedunfilled() {
  int i,j,k,x2,y2,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    for(k=0;k<4;k++) {
      x2=i+dx[k],y2=j+dy[k];
      if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]!=BLOCKED) goto next;
    }
    addmovetoqueue(i,j,BLOCKED);
    ok=1;
  next:;
  }
  return ok;
}

static int level1enclosefinished() {
  int i,j,tot,num,k,l,x2,y2,r=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && m[i][j]>0) {
    genericbfs(i,j,0,&tot,&num,&dummy);
    if(num<1 || num!=tot) goto done;
    for(l=0;l<qe;l+=2) {
      for(k=0;k<4;k++) {
        x2=q[l]+dx[k],y2=q[l+1]+dy[k];
        if(x2<0 || y2<0 || x2>=x || y2>=y) continue;
        if(m[x2][y2]==UNFILLED) addmovetoqueue(x2,y2,BLOCKED),r=1;
      }
    }
  done:
    cleanupbfs();
    if(r) return 1;
  }
  return 0;
}

static int level1hint() {
  if(level1surround1()) return 1;
  if(level12x2()) return 1;
  if(level1twonumberedneighbours()) return 1;
  if(level1enclosedunfilled()) return 1;
  if(level1enclosefinished()) return 1;
  return 0;
}

/*  if there is an unfilled cell which will either:
    - join two islands each containing a numbered cell
    - join two islands, where one has a numbered cell
      and the combination of them exceeds the number
    fill the cell with blocked */
static int level2neighbourtodifferentregions() {
  int i,j,k,res,x2,y2,l;
  int nearid[4],nn;
  if(!markallislandswithid()) return 0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    for(res=1,nn=0,k=0;k<4;k++) {
      x2=i+dx[k],y2=j+dy[k];
      if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]>=EMPTY) {
        for(l=0;l<nn;l++) if(nearid[l]==islandmap[x2][y2]) goto neste;
        res+=islandsize[nearid[nn++]=islandmap[x2][y2]];
      }
    neste:;
    }
    if(nn<2) continue;
    /*  try to find either: two numbered islands or one numbered exceeding */
    for(k=0;k<nn-1;k++) for(l=k+1;l<nn;l++) if(islandnum[nearid[k]] || islandnum[nearid[l]]) {
      x2=islandnum[nearid[k]]>islandnum[nearid[l]]?islandnum[nearid[k]]:islandnum[nearid[l]];
      if((islandnum[nearid[k]] && islandnum[nearid[l]]) || (res>x2)) {
        cleanupbfs();
        addmovetoqueue(i,j,BLOCKED);
        return 1;
      }
    }
  }
  cleanupbfs();
  return 0;
}

/*  level 2 (easy): find a region of blocked such that it has only one unfilled
    neighbour and there are other blocked somewhere on the board */
static int level2growblocked() {
  int i,j,k,l,tot,num,qs2,x2,y2,bx,by;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==BLOCKED && !visit[i][j]) {
    qs2=qs;
    genericbfs(i,j,1,&tot,&num,&dummy);
    if(tot==x*y-totalnum) goto done;
    for(bx=by=-1,k=qs2;k<qe;k+=2) for(l=0;l<4;l++) {
      x2=q[k]+dx[l],y2=q[k+1]+dy[l];
      if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]==UNFILLED) {
        if(bx>-1 && (bx!=x2 || by!=y2)) goto next;  /*  found 2 unfilled neighbours, skip this region */
        else if(bx==-1) bx=x2,by=y2;
      }
    }
    if(bx>-1) {
      cleanupbfs();
      addmovetoqueue(bx,by,BLOCKED);
      return 1;
    }
  next:;
  }
done:
  cleanupbfs();
  return 0;
}

/*  level 2 (easy): find a region of island such that is has only one unfilled
    neighbour and such that the island hasn't reached its size */
static int level2growisland() {
  int i,j,k,l,tot,num,qs2,x2,y2,bx,by;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>=EMPTY && !visit[i][j]) {
    qs2=qs;
    genericbfs(i,j,0,&tot,&num,&dummy);
    if(num>0 && tot==num) continue;
    for(bx=by=-1,k=qs2;k<qe;k+=2) for(l=0;l<4;l++) {
      x2=q[k]+dx[l],y2=q[k+1]+dy[l];
      if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]==UNFILLED) {
        if(bx>-1 && (bx!=x2 || by!=y2)) goto next;  /*  found 2 unfilled neighbours, skip this region */
        else if(bx==-1) bx=x2,by=y2;
      }
    }
    if(bx>-1) {
      cleanupbfs();
      addmovetoqueue(bx,by,EMPTY);
      return 1;
    }
  next:;
  }
  cleanupbfs();
  return 0;
}

/*  check if there is an enclosing with n cells, all unfilled and island with one
    numbered cell=n. start search from unfilled cells to avoid finalized islands */
static int level2enclosing() {
  int i,j,k,tot,num,qs2,x2,y2;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && m[i][j]==UNFILLED) {
    qs2=qs;
    genericbfs(i,j,2,&tot,&num,&dummy);
    if(num<1 || tot!=num) continue;
    for(k=qs2;k<qe;k+=2) if(m[x2=q[k]][y2=q[k+1]]==UNFILLED) {
      cleanupbfs();
      addmovetoqueue(x2,y2,EMPTY);
      return 1;
    }
  }
  cleanupbfs();
  return 0;
}

static int level2hint() {
  if(level2neighbourtodifferentregions()) return 1;
  if(level2growblocked()) return 1;
  if(level2growisland()) return 1;
  if(level2enclosing()) return 1;
  return 0;
}

/*  if a region with n-1 cells has two cells to grow to, and an unfilled
    cell is adjacent to both these cells, mark it blocked */
static int level3nminus1() {
  int i,j,k,l,qs2,tot,num,x2,y2,x3,y3,x4,y4;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>1) {
    qs2=qs;
    genericbfs(i,j,0,&tot,&num,&dummy);
    if(num-1!=tot) continue;
    for(x3=y3=x4=y4=-1,k=qs2;k<qe;k+=2) for(l=0;l<4;l++) {
      x2=q[k]+dx[l],y2=q[k+1]+dy[l];
      if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]==UNFILLED) {
        if(x3<0) x3=x2,y3=y2;
        else if(x4<0 && (x3!=x2 || y3!=y2)) x4=x2,y4=y2;
        else if((x3!=x2 || y3!=y2) && (x4!=x2 || y4!=y2)) goto next;
      }
    }
    if(x3<0 || x4<0) continue;
    /*  find a cell which borders to both (x3,y3) and (x4,y4) by
        trying all neighbours to (x3,y3) and checking if one of them
        is unfilled and a neighbour of (x4,y4) */
    for(k=0;k<4;k++) {
      x2=x3+dx[k],y2=y3+dy[k];
      if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]!=UNFILLED) continue;
      if(abs(x2-x4)+abs(y2-y4)==1) {
        cleanupbfs();
        addmovetoqueue(x2,y2,BLOCKED);
        return 1;
      }
    }
  next:;
  }
  cleanupbfs();
  return 0;
}

/*  find all unfilled cells not reachable by any current island. */
/*  warning: slightly complex algorithm ahead:
    at the start, mark all unfilled cells as unreached
    mark all islands with their own id
    for each island with number n and current size m:
      do a bfs with each island square at start point
      (max depth of search: n-m)
      mark each examined square as reached.
      in the bfs, it's not allowed to visit neighbours of other islands.
    at the end of the search, all cells marked as unreached
    are really unreached */
/*  implementation details: need to reset visit array before each
    island bfs. */
static int level3blockunreached() {
  int i,steps,j,k,id,tot,num,qe2,cx,cy,x2,y2,l,ok=0,x3,y3;
  if(!markallislandswithid()) return 0; /*  generate island id structure */
  for(i=0;i<x;i++) for(j=0;j<y;j++) reach[i][j]=0;  /*  mark as unreachable */
  /*  for each numbered island: do bfs from it */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>0) {
    id=islandmap[i][j];
    if(id<0) return 0;
    /*  preliminary bfs to find the cells of the island */
    genericbfs(i,j,0,&tot,&num,&dummy);
    if(num<0 || tot>num) return 0; /*  bail out of solver at any inconsistency */
    /*  reset queue in order to start the search with the entire current queue as
        start seed */
    qs=0;
    steps=num-tot;
    qe2=qe;
    while(steps--) {
      while(qs<qe) {
        cx=q[qs++],cy=q[qs++];
        for(k=0;k<4;k++) {
          x2=cx+dx[k],y2=cy+dy[k];
          if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2] || m[x2][y2]==BLOCKED) continue;
          /*  check if x2,y2 is adjacent to another numbered island */
          for(l=0;l<4;l++) {
            x3=x2+dx[l],y3=y2+dy[l];
            if(x3<0 || y3<0 || x3>=x || y3>=y || (x2==x3 && y2==y3) || m[x3][y3]<EMPTY) continue;
            if(islandmap[x3][y3]<islandidnum && islandmap[x3][y3]!=id) goto next;
          }
          /*  ok, we can visit square */
          visit[x2][y2]=reach[x2][y2]=1;
          q[qe2++]=x2; q[qe2++]=y2;
        next:;
        }
      }
      qe=qe2;
    }
    cleanupbfs();
  }
  /*  by now, all unvisited cells are non-reachable! set them to blocked */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED && !reach[i][j]) {
    addmovetoqueue(i,j,BLOCKED),ok=1;
  }
  return ok;
}

/*  find an articulation point for island, which causes one of the following if blocked:
    - there will exist 2x2 blocked
    - there will be an enclosing with disconnected unnumbered island
    - one numbered island with not enough room to grow
    - TODO several numbered islands without combined room to grow?
*/
static int level3articulationpointblank() {
  int i,j,k,x2,y2,tot,num,islands,xx,yy,count,u,v;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    /*  assume cell is blocked! check for illegal stuff */
    m[i][j]=BLOCKED;
    /*  do bfs, scan each neighbouring area and check for illegal stuff */
    for(k=0;k<4;k++) {
      x2=i+dx[k],y2=j+dy[k];
      if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]!=BLOCKED) {
        genericbfs(x2,y2,2,&tot,&num,&islands);
        if((num==0 && islands>0) || (num>0 && tot<num)) {
          /*  found an isolated numbered cell, so we have an articulation point
              and the cell must be blank OR we found one numbered cell without
              enough room to grow */
          cleanupbfs();
          m[i][j]=UNFILLED;
          addmovetoqueue(i,j,EMPTY);
          return 1;
        }
        if(!islands) {
          /*  check if there is 2x2 */
          for(xx=0;xx<x-1;xx++) for(yy=0;yy<y-1;yy++) {
            for(count=u=0;u<2;u++) for(v=0;v<2;v++)
              if(m[xx+u][yy+v]==BLOCKED || visit[xx+u][yy+v]) count++;
            if(count>3) {
              /*  found 2x2 blocked */
              cleanupbfs();
              m[i][j]=UNFILLED;
              addmovetoqueue(i,j,EMPTY);
              return 1;
            }
          }
        }
      }
      cleanupbfs();
    }
    m[i][j]=UNFILLED;
  }
  return 0;
}

/*  find an unfinished numbered island which can only grow to exactly its size,
    and finish it. allow it to eat nearby unnumbered islands */
static int level3maximalislandgrow() {
  int i,j,cx,cy,k,x2,y2,tot,num,islands,l,x3,y3,id;
  if(!markallislandswithid()) return 0;
  /*  try every island */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>0) {
    id=islandmap[i][j];
    /*  bfs from start position, only through empty+number */
    genericbfs(i,j,0,&tot,&num,&islands);
    if(islands==num) continue;  /*  already finished */
    if(num<1) return 0; /*  bail out on error */
    qs=0;
    /*  try to expand it */
    while(qs<qe) {
      cx=q[qs++],cy=q[qs++];
      for(k=0;k<4;k++) {
        x2=cx+dx[k],y2=cy+dy[k];
        if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]==BLOCKED || visit[x2][y2]) continue;
        if(m[x2][y2]==UNFILLED) {
          /*  check if cell borders to another numbered island */
          for(l=0;l<4;l++) {
            x3=x2+dx[l],y3=y2+dy[l];
            if(x3<0 || y3<0 || x3>=x || y3>=y || visit[x3][y3] || m[x3][y3]<EMPTY) continue;
            if(islandmap[x3][y3]>=0 && islandmap[x3][y3]<islandidnum && islandmap[x3][y3]!=id) goto next;
          }
        }
        tot++;
        if(tot>num) goto cancel;
        q[qe++]=x2,q[qe++]=y2;
        visit[x2][y2]=1;
      next:;
      }
    }
    /*  fill it in */
    for(k=0;k<qe;k+=2) if(m[q[k]][q[k+1]]==UNFILLED) {
      addmovetoqueue(q[k],q[k+1],EMPTY);
    }
    cleanupbfs();
    return 1;
  cancel:
    cleanupbfs();
  }
  return 0;
}

/*  used by level4connectborder, level3dominatingconnection */
static int wallid[MAXS][MAXS];

/*  max number of blanks allowed in enclosing */
#define MAXCHECK 16
/*  check if a contains all elements of b */
static int dominates(int conn[MAXCHECK][4],int conno[MAXCHECK],int a,int b) {
  int na=conno[a],nb=conno[b],pa=0,pb=0;
  while(pa<na && pb<nb) {
    if(conn[b][pb]<conn[a][pa]) return 0;
    else if(conn[b][pb]==conn[a][pa]) pa++,pb++;
    else pa++;
  }
  return pb==nb;
}

/*  return 1 if cell contains a foreign unfilled */
static int dominateoutsideblank(int *checkx,int *checky,int px,int py,int num) {
  int i;
  if(px<0 || py<0 || px>=x || py>=y || m[px][py]!=UNFILLED) return 0;
  for(i=0;i<num;i++) if(px==checkx[i] && py==checky[i]) return 0;
  return 1;
}

/*  NB, dubious heuristic. not sure if it is correct. */
/*  find an enclosing with only one number, such that the size of the enclosing
    is (number+1) with at least two blanks. then, try all ways of putting
    blocked. if all the possible blocked connections are possible using only
    one blocked, then that cell must be blocked. */
/*  to increase chance of correctness: for each blank inside enclosure,
    check if all 2x2 cells containing it does not contain blanks outside of
    enclosure */
static int level3dominatingconnection() {
  int i,j,k,l,z,islands,num,tot,idno=0,qs2,px,py,qx,qy,maxsize,winner=-1;
  /*  connections! for i, all elements in conn[i][j] are connected */
  static int conn[MAXCHECK][4];
  static int conno[MAXCHECK]; /*  number of connections */
  /*  coordinates of blanks */
  static int checkx[MAXCHECK],checky[MAXCHECK];
  int blanks; /*  number of blanks */
  /*  generate connection map for blocked */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==BLOCKED && !visit[i][j]) {
    qs2=qs;
    genericbfs(i,j,1,&tot,&num,&islands);
    while(qs2<qe) wallid[q[qs2]][q[qs2+1]]=idno,qs2+=2;
    idno++;
  }
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>0 && !visit[i][j]) {
    /*  bfs through non-blocked */
    qs2=qs;
    genericbfs(i,j,2,&tot,&num,&islands);
    if(num<1) continue;
    if(tot!=num+1) continue;
    if(MAXCHECK<tot-islands) continue;
    /*  TODO check all 2x2 */
    /*  go through all blanks and store connections */
    for(blanks=0,k=qs2;k<qe;k+=2) {
      px=q[k],py=q[k+1];
      if(m[px][py]!=UNFILLED) continue;
      checkx[blanks]=px; checky[blanks]=py;
      conno[blanks]=0;
      for(z=0;z<4;z++) {
        qx=px+dx[z],qy=py+dy[z];
        if(qx<0 || qy<0 || qx>=x || qy>=y || m[qx][qy]!=BLOCKED) continue;
        for(l=0;l<conno[blanks];l++) if(conn[blanks][l]==wallid[qx][qy]) goto nonew;
        conn[blanks][conno[blanks]++]=wallid[qx][qy];
      nonew:;
      }
      if(conno[blanks]>1) qsort(conn[blanks],conno[blanks],sizeof(int),compi);
      blanks++;
    }
    /*  check for outside blanks. if they exist, they invalidate the heuristic */
    for(k=0;k<blanks;k++) {
      if(dominateoutsideblank(checkx,checky,checkx[k]-1,checky[k]-1,blanks)) goto failed;
      if(dominateoutsideblank(checkx,checky,checkx[k]+1,checky[k]-1,blanks)) goto failed;
      if(dominateoutsideblank(checkx,checky,checkx[k]-1,checky[k]+1,blanks)) goto failed;
      if(dominateoutsideblank(checkx,checky,checkx[k]+1,checky[k]+1,blanks)) goto failed;
    }
    /*  try to find an island which dominates the others */
    maxsize=0;
    for(k=0;k<blanks;k++) if(maxsize<conno[k]) maxsize=conno[k];
    for(k=0;k<blanks;k++) if(maxsize==conno[k]) {
      for(l=0;l<blanks;l++) if(k!=l && !dominates(conn,conno,k,l)) goto nodominate;
      /*  we have a winner */
      if(winner>-1) { winner=-1; goto failed; }
      winner=k;
    nodominate:;
    }
    if(winner>-1) {
      addmovetoqueue(checkx[winner],checky[winner],BLOCKED);
      cleanupbfs();
      return 1;
    }
  failed:;
  }
  cleanupbfs();
  return 0;
}
#undef MAXCHECK

static int level3hint() {
  if(level3nminus1()) return 1;
  if(level3blockunreached()) return 1;
  if(level3articulationpointblank()) return 1;
  if(level3maximalislandgrow()) return 1;
  if(level3dominatingconnection()) return 1;
  return 0;
}

/*  test every square to see if it is an articulation point for
    blocked: by assuming it is white, check if:
    - there will be a disconnected piece of black */
static int level4articulationpointblocked() {
  int i,j,k,x2,y2,tot,num,res,qs2,l,count;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    /*  assume cell is empty and check for illegal stuff */
    m[i][j]=EMPTY;
    /*  do bfs from each neighbouring area and check for disconnection */
    for(res=k=0;k<4;k++) {
      x2=i+dx[k],y2=j+dy[k];
      if(x2<0 || y2<0 || x2>=x || y2>=y || (m[x2][y2]!=UNFILLED && m[x2][y2]!=BLOCKED)) continue;
      /*  bfs through blocked and unfilled */
      qs2=qs;
      genericbfs(x2,y2,3,&tot,&num,&dummy);
      /*  check the actual number of blocked encountered */
      for(count=0,l=qs2;l<qe;l+=2) if(m[q[l]][q[l+1]]==BLOCKED) count++;
      if(count && res) {
        /*  found articulation point! */
        cleanupbfs();
        m[i][j]=UNFILLED;
        addmovetoqueue(i,j,BLOCKED);
        return 1;
      }
      if(count) res=count;
    }
    cleanupbfs();
    m[i][j]=UNFILLED;
  }
  return 0;
}

/*  check if an island can grow into a cell such that it blocks the growth of another island.
    then the offending cell must be blocked */
static int level4illegalgrow() {
  int i,j,k,x2,y2,l,id,tot,num,x3,y3,cx,cy,id2,d,found,e;
  if(!markallislandswithid()) return 0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    /*  bordering to numbered island? */
    /*  to avoid tricky cases, make sure no other island borders to our cell */
    x2=y2=-1;
    for(found=-1,k=0;k<4;k++) {
      x3=i+dx[k],y3=j+dy[k];
      if(x3<0 || y3<0 || x3>=x || y3>=y) continue;
      if(m[x3][y3]>=EMPTY) {
        if((id=islandmap[x3][y3])>=islandidnum) { found=-2; break; }
        if(found<0) found=id;
        else if(found>-1 && found!=id) { found=-2; break; }
        x2=x3,y2=y3;
      }
    }
    if(found<0) continue;
    /*  now, assume that island at (x2,y2) has grown to this cell.
        check if nearby islands cannot grow enough */
    /*  for now, do it the slow way. for each other island, grow it
        (it's permitted to grow through unnumbered islands). */
    id=islandmap[x2][y2];
    m[i][j]=EMPTY; islandmap[i][j]=id;
    /*  manhattan distance heuristic: don't grow island if it's too far away from our change */
    for(k=0;k<x;k++) for(l=0;l<y;l++) if(m[k][l]>0 && (id2=islandmap[k][l])!=id && manhattandist(i,j,k,l)<m[k][l]) {
      genericbfs(k,l,0,&tot,&num,&dummy);
      if(tot==num) goto abortsearch;  /*  ignore completed island */
      qs=0;
      while(qs<qe) {
        cx=q[qs++],cy=q[qs++];
        for(d=0;d<4;d++) {
          x2=cx+dx[d],y2=cy+dy[d];
          if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]==BLOCKED || visit[x2][y2]) continue;
          /*  reject if other islands are neighbours */
          for(e=0;e<4;e++) {
            x3=x2+dx[e],y3=y2+dy[e];
            if(x3<0 || y3<0 || x3>=x || y3>=y || visit[x3][y3] || m[x3][y3]<EMPTY) continue;
            if(islandmap[x3][y3]!=id2 && islandmap[x3][y3]<islandidnum) goto illegal;
          }
          tot++;
          if(tot>=num) goto abortsearch;  /*  managed to grow the island */
          q[qe++]=x2; q[qe++]=y2;
          visit[x2][y2]=1;
        illegal:;
        }
      }
      /*  found a cell we can't grow into! */
      m[i][j]=UNFILLED; islandmap[i][j]=-1;
      cleanupbfs();
      addmovetoqueue(i,j,BLOCKED);
      return 1;
    abortsearch:
      cleanupbfs();
    }
    m[i][j]=UNFILLED; islandmap[i][j]=-1;
  }
  return 0;
}

/*  find a cell such that if wall is placed, we will block a numbered island's
    growth. then, this cell must be island. */
/*  (this is a partial substitute for island intersection (without combining
    with unnumbered islands), except that this one runs in polynomial time) */
static int level4advancedarticulationisland() {
  int i,j,k,l,tot,num,d,e,x2,y2,cx,cy,x3,y3,id;
  if(!markallislandswithid()) return 0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    /*  assume it's blocked */
    m[i][j]=BLOCKED;
    /*  manhattan distance heuristic: don't grow island if it's too far away from our change */
    for(k=0;k<x;k++) for(l=0;l<y;l++) if(m[k][l]>0 && manhattandist(i,j,k,l)<m[k][l]) {
      id=islandmap[k][l];
      genericbfs(k,l,0,&tot,&num,&dummy);
      if(tot==num) goto abortsearch;
      qs=0;
      while(qs<qe) {
        cx=q[qs++],cy=q[qs++];
        for(d=0;d<4;d++) {
          x2=cx+dx[d],y2=cy+dy[d];
          if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2] || m[x2][y2]==BLOCKED) continue;
          for(e=0;e<4;e++) {
            x3=x2+dx[e],y3=y2+dy[e];
            if(x3<0 || y3<0 || x3>=x || y3>=y || visit[x3][y3] || m[x3][y3]<EMPTY) continue;
            if(islandmap[x3][y3]!=id && islandmap[x3][y3]<islandidnum) goto illegal;
          }
          tot++;
          if(tot>=num) goto abortsearch;
          q[qe++]=x2; q[qe++]=y2;
          visit[x2][y2]=1;
        illegal:;
        }
      }
      /*  can't grow, this cell needs to be blank */
      m[i][j]=UNFILLED;
      cleanupbfs();
      addmovetoqueue(i,j,EMPTY);
      return 1;
    abortsearch:
      cleanupbfs();
    }
    m[i][j]=UNFILLED;
  }
  return 0;
}

static int dd2[2][6]={{ 1, 0,1, 1, 0, 1},{ 1, 0,0, 0,1,0}};
static int dx2[2][6]={{ 0,-1,0, 1, 1, 1},{ 0,-1,0,-1,0,0}};
static int dy2[2][6]={{-1, 0,0,-1, 0, 0},{-1, 0,0, 1,1,1}};

/*  return bitmask containing adjacent region id's */
/*  follow edge such that the next edge is adjacent to two cells:
    - one visited (unfilled/empty)
    - one unvisited (blocked)
    (border edge is not allowed) */
/*  x1,y1: coordinate of unfilled cell, x2,y2: wall */
/*  routine will not visit the numbered cell */
static int followedge(int x1,int y1,int x2,int y2) {
  int pd=-1,px=-1,py=-1;  /*  previous cell */
  int cd=0,cx=0,cy=0; /*  current cell */
  int nd,nx,ny; /*  next */
  int ret=0;    /*  resulting bitmask */
  int d,haswall,hasisland;
  if(x1==x2) cd=0,cx=x1,cy=y1<y2?y2:y1;
  if(y1==y2) cd=1,cx=x1<x2?x2:x1,cy=y1;
  while(1) {
    /*  check if we are next to the numbered tile */
    if(cd==0 && (m[cx][cy-1]>0 || m[cx][cy]>0)) return ret;
    if(cd==1 && (m[cx-1][cy]>0 || m[cx][cy]>0)) return ret;
    /*  extract wall id and or it */
		/* TODO investigate whether wallid can be negative */
    if(cd==0 && m[cx][cy-1]==BLOCKED) ret|=1<<wallid[cx][cy-1];
    if(cd==0 && m[cx][cy]==BLOCKED) ret|=1<<wallid[cx][cy];
    if(cd==1 && m[cx-1][cy]==BLOCKED) ret|=1<<wallid[cx-1][cy];
    if(cd==1 && m[cx][cy]==BLOCKED) ret|=1<<wallid[cx][cy];
    /*  find next edge */
    for(d=0;d<6;d++) {
      nd=dd2[cd][d];
      nx=cx+dx2[cd][d];
      ny=cy+dy2[cd][d];
      if(nd==pd && nx==px && ny==py) continue;
      if(nd==0 && (ny==0 || ny==y)) continue;
      if(nd==1 && (nx==0 || nx==x)) continue;
      /*  check if there is one wall (unvisited) and one unfilled/empty (visited) */
      haswall=hasisland=0;
      if(nd==0 && (visit[nx][ny-1] || visit[nx][ny])) hasisland=1;
      if(nd==1 && (visit[nx-1][ny] || visit[nx][ny])) hasisland=1;
      if(nd==0 && (!visit[nx][ny-1] || !visit[nx][ny])) haswall=1;
      if(nd==1 && (!visit[nx-1][ny] || !visit[nx][ny])) haswall=1;
      if(haswall && hasisland) break;
    }
    if(d<6) {
      pd=cd; px=cx; py=cy;
      cd=nd; cx=nx; cy=ny;
      continue;
    }
    error("edge following went astray\n");
  }
}

static int level4connectborder() {
  int i,j,num,dummy,okborder,qs2,k,l,regions,cx,cy,x2,y2,d,mask[2],wallnear,qe2;
  /*  first, find a numbered cell not adjacent to the border */
  /*  do not cleanupbfs */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>0 && i && j && i<x-1 && j<y-1) {
    /*  check if the numbered cell is next to a wall. if no, reject */
    if(m[i-1][j]!=BLOCKED && m[i+1][j]!=BLOCKED && m[i][j-1]!=BLOCKED && m[i][j+1]!=BLOCKED) continue;
    /*  check if the region contains only one numbered cell */
    genericbfs(i,j,2,&dummy,&num,&dummy);
    if(num<1) { cleanupbfs(); continue; }
    /*  check if the region is next to a border, and that all border cells are
        unfilled */
    /*  also check that at most 2 pairs of border cells unfilled/wall */
    for(wallnear=okborder=k=0;k<qe;k+=2) {
      if(!q[k] || !q[k+1] || q[k]==x-1 || q[k+1]==y-1) {
        if(m[q[k]][q[k+1]]==EMPTY) { okborder=0; goto failed; }
        okborder=1;
        for(d=0;d<4;d++) {
          x2=q[k]+dx[d]; y2=q[k+1]+dy[d];
          if(x2 && y2 && x2<x-1 && y2<y-1) continue;
          if(m[x2][y2]!=BLOCKED) continue;
          wallnear++;
        }
      }
    }
  failed:
    if(!okborder || wallnear!=2) { cleanupbfs(); continue; }
    /*  now, check that the region cannot be split int two (or more) by
        pretending that the numbered cell is a wall */
    qs2=qe2=qe;
    for(d=0;d<4;d++) {
      x2=i+dx[d]; y2=j+dy[d];
      if(m[x2][y2]!=BLOCKED) {
        /*  do bfs among empty and unfilled, only within cells having
            visit[][]==1 */
        q[qe2++]=x2; q[qe2++]=y2;
        while(qs2<qe2) {
          cx=q[qs2++]; cy=q[qs2++];
          for(d=0;d<4;d++) {
            x2=cx+dx[d]; y2=cy+dy[d];
            if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2]!=1) continue;
            if(m[x2][y2]!=UNFILLED && m[x2][y2]!=EMPTY) continue;
            visit[x2][y2]=2;
            q[qe2++]=x2; q[qe2++]=y2;
          }
        }
        break;
      }
    }
    for(d=0;d<4;d++) {
      x2=i+dx[d]; y2=j+dy[d];
      if(m[x2][y2]!=BLOCKED && visit[x2][y2]==1) break;
    }
    if(d<4) { cleanupbfs(); continue; }
    qs2=qe;
    /*  visit all remaining wall/unfilled and assign each region a unique id */
    for(k=0;k<x;k++) for(l=0;l<y;l++) wallid[k][l]=-1;
    for(regions=k=0;k<x;k++) for(l=0;l<y;l++) if(!visit[k][l] && m[k][l]==BLOCKED) {
      if(regions==31) {
        /*  too many regions, so ignore */
        cleanupbfs();
        goto failed2;
      }
      /*  manual bfs here */
      q[qe++]=k; q[qe++]=l;
      visit[k][l]=1;
      wallid[k][l]=regions;
      while(qs<qe) {
        cx=q[qs++]; cy=q[qs++];
        for(d=0;d<4;d++) {
          x2=cx+dx[d]; y2=cy+dy[d];
          if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2] || (m[x2][y2]!=UNFILLED && m[x2][y2]!=BLOCKED)) continue;
          q[qe++]=x2; q[qe++]=y2;
          visit[x2][y2]=1;
          wallid[x2][y2]=regions;
        }
      }
      regions++;
    }
    /*  unvisit region */
    for(k=qs2;k<qe;k+=2) visit[q[k]][q[k+1]]=0;
    qs=qe=qs2;
    /*  traverse edges and collect adjacent wall id's */
    k=mask[0]=mask[1]=0;
    /*  find cell near border */
    for(l=0;l<qe;l+=2) if(!q[l] || !q[l+1] || q[l]==x-1 || q[l+1]==y-1) {
      /*  check if it is adjacent to wall */
      for(d=0;d<4;d++) {
        x2=q[l]+dx[d]; y2=q[l+1]+dy[d];
        if(x2<0 || y2<0 || x2>=x || y2>=y) continue;
        if(m[x2][y2]==BLOCKED && (!x2 || !y2 || x2==x-1 || y2==y-1)) {
          if(k==2) {
            /*  more than two unfilled near border. ignore this region */
            cleanupbfs();
            goto failed2;
          }
          mask[k++]=followedge(q[l],q[l+1],x2,y2);
          break;
        }
      }
    }
    /*  check if there are no id's in common */
    if(!(mask[0]&mask[1])) {
      /*  fill in all border cells */
      for(i=0;i<qe;i+=2) if(!q[i] || !q[i+1] || q[i]==x-1 || q[i+1]==y-1) {
        if(m[q[i]][q[i+1]]!=UNFILLED) error("at island with size %d, tried to put wall fill in the border cell %d %d, but it wasn't unfilled\n",i,j,num,q[i],q[i+1]);
        addmovetoqueue(q[i],q[i+1],BLOCKED);
      }
      cleanupbfs();
      return 1;
    }
  failed2:
    cleanupbfs();
  }
  return 0;
}

static int level4islandmatchseparate() {
  /*  bitmask for storing bitmasks: islandmask[id][] stores which islands
      id can be reached by. for id with numbered cells, the mask only contains
      its own id */
  static unsigned int islandmask[MAXISLAND][(MAXISLAND+31)>>5];
  static int compmask[(MAXISLAND+31)>>5];
  /*  number of times an unnumbered cell has been reached by a numbered island */
  static int numreach[MAXISLAND];
  int i,j,steps,id,tot,num,qe2,cx,cy,k,l,x2,y2,x3,y3,left,count,ok=0,illegal;
  /*  initialize islands! for each island, record the following:
      - island size
      - island number (if it has)
      - island id
      - also, initialize a bitmask which shall hold which numbered island
        can reach this one
  */
  if(!markallislandswithid()) return 0;
  /*  initialize the bitmasks */
  for(i=0;i<islandid;i++) for(j=0;j<((islandid+31)>>5);j++) islandmask[i][j]=0;
  /*  a numbered island can reach itself */
  for(i=0;i<islandidnum;i++) islandmask[i][i>>5]|=1U<<(i&31);
  for(i=0;i<islandid;i++) numreach[i]=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) reach[i][j]=0;
  /*  bfs from each numbered island: entire island is put as starting position
      into queue, and it is allowed to move through unnumbered islands (but
      not allowed to move next to other numbered islands). allowed distance:
      number-island size. if this island reaches an unnumbered island, then
      set bit id in its bitmask, where id is the numbered island's id. (this
      routine is "safe" and will set too many bitmasks, for instance allowing
      passing through other islands to reach islands which it cannot reach in
      reality, making the bitmasks too large, and making it harder than
      necessary to obtain mask1&mask2==0.) */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>0) {
    id=islandmap[i][j];
    if(id<0) error("at %d %d, island without id\n",i,j);
    /*  preliminary bfs to find the cells of the island */
    genericbfs(i,j,0,&tot,&num,&dummy);
    if(num<0 || tot>num) return 0; /*  illegal state, terminate */
    /*  reset queue in order to start the search with the entire current queue as
        start seed */
    qs=0; qe2=qe;
    left=num-tot+1;  /*  allow +1 for entering an island */
    steps=0;
    while(left--) {
      while(qs<qe) {
        cx=q[qs++]; cy=q[qs++];
        for(k=0;k<4;k++) {
          x2=cx+dx[k],y2=cy+dy[k];
          if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]==BLOCKED) continue;
          /*  ignore already visited cell, unless it is island */
          if(visit[x2][y2] && m[x2][y2]==UNFILLED) continue;
          /*  check if x2,y2 is adjacent to another numbered island */
          for(illegal=l=0;l<4;l++) {
            x3=x2+dx[l],y3=y2+dy[l];
            if(x3<0 || y3<0 || x3>=x || y3>=y || (x2==x3 && y2==y3) || m[x3][y3]<EMPTY) continue;
            if(islandmap[x3][y3]!=id && tot+steps+islandsize[islandmap[x3][y3]]+1>num) {
              illegal=1;
            }
            if(islandmap[x3][y3]<islandidnum && islandmap[x3][y3]!=id) goto next;
          }
          /*  ok, we can visit square */
          /*  if we are inside another island and it can be joined while still
              obeying the size restrictions, update mask */
          if(m[x2][y2]==EMPTY && islandmap[x2][y2]!=id && tot+steps+islandsize[islandmap[x2][y2]]<=num) {
            islandmask[islandmap[x2][y2]][id>>5]|=1U<<(id&31);
            numreach[islandmap[x2][y2]]++;
          }
          if(!illegal) reach[x2][y2]=1;
          if(!visit[x2][y2]) {
            visit[x2][y2]=1;
            q[qe2++]=x2; q[qe2++]=y2;
          }
        next:;
        }
      }
      qe=qe2;
      steps++;
    }
    cleanupbfs();
  }
  /*  then, for each unfilled cell, if it has 2 neighbouring island cells with
      bitmask_a & bitmask_b == 0, set to wall */
  /*  i guess i could work for 3 neighbours also, but i'm lazy */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    for(k=0;k<((islandid+31)>>5);k++) compmask[k]=(unsigned)(-1);
    for(count=k=0;k<4;k++) {
      x2=i+dx[k]; y2=j+dy[k];
      if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]<EMPTY) continue;
      id=islandmap[x2][y2];
      for(l=0;l<((islandid+31)>>5);l++) compmask[l]&=islandmask[id][l];
      count++;
    }
    if(count!=2) goto nothing;
    for(k=0;k<((islandid+31)>>5);k++) if(compmask[k]) goto nothing;
    addmovetoqueue(i,j,BLOCKED);
    ok=1;
  nothing:
    /*  if an unfilled cell is next to a cell which is reachable in only one
        way, it must be empty */
    if(!reach[i][j]) continue;
    for(k=0;k<4;k++) {
      x2=i+dx[k]; y2=j+dy[k];
      if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]<EMPTY) continue;
      if(numreach[islandmap[x2][y2]]==1) {
        addmovetoqueue(i,j,EMPTY);
        ok=1;
      }
    }
  }
  return ok;
}

static int level4hint() {
  if(level4articulationpointblocked()) return 1;
  if(level4illegalgrow()) return 1;
  if(level4advancedarticulationisland()) return 1;
  if(level4islandmatchseparate()) return 1;
  if(level4connectborder()) return 1;
  return 0;
}

static int lev5bak[MAXS][MAXS];
static int lev5alt1[MAXS][MAXS];
static int lev5alt2[MAXS][MAXS];

/*  copy board from a to b */
static void copyboard(int a[MAXS][MAXS],int b[MAXS][MAXS]) {
  int i,j;
  for(i=0;i<x;i++) for(j=0;j<y;j++) b[i][j]=a[i][j];
}

/*  recalculate st[][] on a fresh board */
static void recalcboard() {
  int i,j;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>=EMPTY) recalccompleteness(i,j,0);
  cleanupbfs();
}

/*  do greedy */
/*  return 0 if no faults found, -1 if contradiction */
static int dogreedy(int lev) {
  int r;
  while(1) {
    r=deepverifyboard();
/*    logprintboard();
    logprintf("in loop, result from deepverifyboard: %d\n",r);*/
    if(r<0) return -1;
    if(r>0) return 0;
    if(level1hint()) goto theend;
    if(lev>1 && level2hint()) goto theend;
    if(lev>2 && level3hint()) goto theend;
    if(lev>3 && level4hint()) goto theend;
    break;
  theend:
/*    printf("%d moves in q\n",mqe/3);*/
    silentexecutemovequeue();
  }
  return 0;
}

/*  assume that a cell is white, fill greedily.
    assume that the same cell is black, fill greedily.
    if one of the assumtions results in a contradiction,
    accept the other initial assumption.
    if no contradiction, accept all cells that were filled in similarly.
    only level 1-3 heuristics can be used in greedy.
    parameter lev indicates maximal subheuristic level (max 4) */
static int level5contradiction(int lev) {
  int z,r,l,k,oldsp=getstackpos();
  static int i=0,j=0;
	if(i>=x) i=0; if(j>=y) j=0;
  for(z=0;z<x*y;z++) {
    if(m[i][j]==UNFILLED) {
      /*  assume wall */
      copyboard(m,lev5bak);
      m[i][j]=BLOCKED;
      updatetoscreen(i,j,0);
      r=dogreedy(lev);
      if(r<0) {
        /*  contradiction! */
        copyboard(lev5bak,m);
        recalcboard();
        setstackpos(oldsp);
        addmovetoqueue(i,j,EMPTY);
        return 1;
      }
      copyboard(m,lev5alt1);
      copyboard(lev5bak,m);
      recalcboard();
      setstackpos(oldsp);
      /*  assume blank */
      m[i][j]=EMPTY;
      recalccompleteness(i,j,0);  /*  update st[][] */
      cleanupbfs();
      for(k=0;k<x;k++) for(l=0;l<y;l++) if(visit[k][l]) logprintf("visit not removed at %d,%d\n",k,l);
      r=dogreedy(lev);
      if(r<0) {
        /*  contradiction! */
        copyboard(lev5bak,m);
        recalcboard();
        setstackpos(oldsp);
        addmovetoqueue(i,j,BLOCKED);
        return 1;
      }
      copyboard(m,lev5alt2);
      copyboard(lev5bak,m);
      recalcboard();
      setstackpos(oldsp);
      for(r=k=0;k<x;k++) for(l=0;l<y;l++) if(lev5alt1[k][l]!=UNFILLED && lev5alt1[k][l]==lev5alt2[k][l] && m[k][l]==UNFILLED)
        addmovetoqueue(k,l,lev5alt1[k][l]),r=1;
      if(r) return 1;
    }
    j++;
    if(j==y) {
      j=0,i++;
      if(i==x) i=0;
    }
  }
  return 0;
}

/*  start of level5tryallislands! hash stuff */

/*  max size of island to attempt */
#define LEVEL5MAXSIZE 13
/*  max number of elements in hash table. must be high enough to
    accomodate all islands of size LEVELMAXSIZE plus a good safety margin */
/*  now the solver suddenly requires a decent amount of RAM */
#define MAXHASH 5100007
static char hash[(MAXHASH+7)/8];  /*  bit i set if element i is taken */
static int hashdata[MAXHASH][LEVEL5MAXSIZE+1];
static int hashcount;
static int level5btrgiveup;
#define HASHMAGIC 257
#define HASHSHIFTLEFT 8
/*  WARNING, select HASHMAGIC and MAXHASH such that its product plus the coordinate
    value never overflows */
static int GETHASH(int *coords,int n) {
  int i,v=0;
  for(i=0;i<n;i++) v=(v*HASHMAGIC+coords[i])%MAXHASH;
  return v;
}
/*  return taken bit: 0: not set, >0: set */
#define HASHBIT(pos) (hash[pos>>3]&(1<<(pos&7)))
/*  return 1 if key is equal to element in position pos */
static int hashcompare(int pos,int *coords,int n) {
  int i;
  if(hashdata[pos][0]!=n) return 0;
  for(i=0;i<n;i++) if(coords[i]!=hashdata[pos][i+1]) return 0;
  return 1;
}
/*  given key, find its position in hash table */
/*  it probes past entries occupied by other keys, and stops at
    either the right key or the first empty entry */
static int gethashpos(int *coords,int n) {
  int pos=GETHASH(coords,n);
  while(1) {
    if(!HASHBIT(pos)) break;
    if(hashcompare(pos,coords,n)) break;
    pos++; if(pos==MAXHASH) pos=0;
  }
  return pos;
}
/*  put an element into the hash table */
static void puthash(int *coords,int n) {
  int pos=gethashpos(coords,n),i;
  hash[pos>>3]|=1<<(pos&7);
  hashdata[pos][0]=n;
  for(i=0;i<n;i++) hashdata[pos][i+1]=coords[i];
  hashcount++;
  if((hashcount<<3)>MAXHASH) level5btrgiveup=1;
}
/*  clears the hash table */
static void inithashdata() {
  memset(hash,0,sizeof(hash));
  hashcount=0;
}

/*  indicate whether cell is included in current island */
static char level5vis[MAXS][MAXS];

/*  check if state is visited, and also inserts it if it wasn't
    return 0 if element doesn't exist, non-zero if it does */
static int level5btrvisited(int *coords,int has) {
  static int c[LEVEL5MAXSIZE];
  int i,j,a,r;
  for(i=0;i<has;i++) c[i]=coords[i*2]+(coords[i*2+1]<<HASHSHIFTLEFT);
  /*  TODO maybe replace insertion sort later with something faster */
  for(i=1;i<has;i++) {
    j=i;
    a=c[j];
    while(j && c[j-1]>a) c[j]=c[j-1],j--;
    c[j]=a;
  }
  a=gethashpos(c,has);
  if(!(r=HASHBIT(a))) puthash(c,has);
  return r;
}

#define NOVALUE -101
#define NOINTERSECT -102
static int btrres5[MAXS][MAXS];

/*  play greedily and intersect result with btrres5 */
void level5btrgreedy() {
  int r,i,j,oldsp=getstackpos();
  r=dogreedy(4);
  if(r>-1) for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]!=btrres5[i][j]) btrres5[i][j]=(btrres5[i][j]==NOVALUE)?m[i][j]:NOINTERSECT;
  while(getstackpos()>oldsp) undo(0);
}

/*  num: size of island when it's full
    has: size of currently grown island
    *coords: pointer to island coordinates (x_0, y_0, x_1, ..., y_has-1),
             has has*2 elements */
static void level5btr(int num,int has,int *coords) {
  int i,j,d,e,px,py,newhas,qx,qy,tot,number,islands;
  /*  check if state is taken (also inserts state if free) */
  if(level5btrgiveup || level5btrvisited(coords,has)) return;
  if(num==has) {
    /*  TODO consider putting wall around island */
/*    logprintf("FULL ISLAND break");
    for(i=0;i<has;i++) logprintf(" [%d %d]",coords[i*2],coords[i*2+1]); logprintf("\n");*/
    level5btrgreedy();
    return;
  }
  for(i=0;i<has;i++) for(d=0;d<4;d++) {
    px=coords[i<<1]+dx[d]; py=coords[(i<<1)+1]+dy[d];
    if(px<0 || py<0 || px>=x || py>=y || m[px][py]!=UNFILLED) continue;
    newhas=has;
    for(e=0;e<4;e++) {
      qx=px+dx[e],qy=py+dy[e];
      if(qx<0 || qy<0 || qx>=x || qy>=y || m[qx][qy]<EMPTY || level5vis[qx][qy]) continue;
      if(m[qx][qy]>EMPTY) { cleanupbfs(); goto badmove; }
      /*  join island */
      genericbfs(qx,qy,0,&tot,&number,&islands);
      if(number>0 || newhas+1+islands>num) { cleanupbfs(); goto badmove; }
      for(j=0;j<qe;j+=2) {
        level5vis[q[j]][q[j+1]]=1;
        coords[newhas<<1]=q[j];
        coords[(newhas<<1)+1]=q[j+1];
        newhas++;
      }
      cleanupbfs();
    }
    coords[newhas<<1]=px; coords[(newhas<<1)+1]=py; level5vis[px][py]=1;
    newhas++;
    if(newhas<=num) {
      m[px][py]=EMPTY;
      updatetoscreen(px,py,0);
      level5btr(num,newhas,coords);
      m[px][py]=UNFILLED;
      updatetoscreen(px,py,0);
    }
  badmove:
    while(newhas>has) {
      newhas--;
      level5vis[coords[newhas<<1]][coords[(newhas<<1)+1]]=0;
    }
  }
}

/*  for every numbered island, try to grow it in all possible ways. for each
    way, assume that the island is like that, then solve greedily. then take
    the intersection
    among all legal states. */
/*  WARNING, take care of the following:
    - don't visit partial states more than once (use some kind of hashing)
    - definitely don't visit fully grown islands more than once.
    - when the island grows into another island, appropriately reduce the number
      of remaining island cells. also check if size is exceeded or we entered
      another numbered island */
static int level5tryallislands() {
  static int i=0,j=0;
  int iter,k,l,tot,num,islands,has,ok=0;
  static int coords[LEVEL5MAXSIZE*2];
	if(i>=x) i=0; if(j>=y) j=0;
  for(iter=0;iter<x*y;iter++) {
    i++; if(i==x) {
      i=0; j++;
      if(j==y) j=0;
    }
    if(m[i][j]>0) {
      /*  only proceed if the island is incomplete and it is numbered */
      genericbfs(i,j,0,&tot,&num,&islands);
      if(num<1 || num==islands || num>LEVEL5MAXSIZE) { cleanupbfs(); continue; }
      /*  init state, prepare btr, init hash */
      for(k=0;k<x;k++) for(l=0;l<y;l++) level5vis[k][l]=0;
      for(k=0;k<qe;k++) coords[k]=q[k];
      for(k=0;k<qe;k+=2) level5vis[q[k]][q[k+1]]=1;
      has=qe>>1;
      cleanupbfs();
      inithashdata();
      /*  init data area for collecting results */
      copyboard(m,lev5alt1);
      for(k=0;k<x;k++) for(l=0;l<y;l++) btrres5[k][l]=NOVALUE;
/*      logprintf("process island at %d %d of num %d current size %d\n",i,j,num,islands);*/
      level5btrgiveup=0;
      level5btr(num,has,coords);
      copyboard(lev5alt1,m);
      recalcboard();
      /*  apply intersection */
      if(!level5btrgiveup) {
        for(k=0;k<x;k++) for(l=0;l<y;l++) if(btrres5[k][l]!=NOINTERSECT && btrres5[k][l]!=UNFILLED && m[k][l]==UNFILLED) {
          /*  TODO NOVALUE will function as an assert when this routine is complete */
          if(btrres5[k][l]==NOVALUE) continue;
          ok=1;
  /*        logprintf("thanks to level5tryallislands, %d %d => %d\n",k,l,btrres5[k][l]);*/
          addmovetoqueue(k,l,btrres5[k][l]);
        }
      } else {
        logprintf("[.]");
      }
      if(ok) return 1;
    }
  }
  return 0;
}
#undef LEVEL5MAXSIZE
#undef HASHBIT
#undef MAXHASH
#undef NOVALUE
#undef NOINTERSECT

static int level5hint() {
  if(level5contradiction(3)) return 1;
  if(level5contradiction(4)) return 1;
  if(level5tryallislands()) return 1;
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
  int colour=0,qs2,tot,num,k,i,j;
  if(forcefullredraw) drawgrid();
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && m[i][j]==BLOCKED) {
    qs2=qs;
    genericbfs(i,j,1,&tot,&num,&dummy);
    for(k=qs2;k<qe;k+=2) drawsolidcell32(q[k],q[k+1],colarray[colour%COLSIZE]);
    colour++;
  }
  /*  display 2x2 blocked */
  for(i=0;i<x-1;i++) for(j=0;j<y-1;j++) if(m[i][j]==BLOCKED && m[i+1][j]==BLOCKED && m[i][j+1]==BLOCKED && m[i+1][j+1]==BLOCKED) {
    drawsolidcell32(i,j,DARKRED32);
    drawsolidcell32(i,j+1,DARKRED32);
    drawsolidcell32(i+1,j,DARKRED32);
    drawsolidcell32(i+1,j+1,DARKRED32);
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
  if(key==undokey) undo(1),usedundo=1;
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
  } else if(key==verifykey) showverify();
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

void nurikabe(char *path,int solve) {
  int event,i,j;
  autosolve=solve;
  loadpuzzle(path);
  initcompleteness();
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>0) recalccompleteness(i,j,1);
  cleanupbfs();
  drawgrid();
  if(autosolve) { autosolver(path); return; }
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
      if(verifyboard()==1) {
				finalizetime(); displayscore(x,y);
        messagebox(1,"You are winner!");
        return;
      }
      break;
    default:
      /*  catch intervals of values here */
      if(event>=EVENT_KEYDOWN && event<EVENT_KEYUP) {
        processkeydown(event-EVENT_KEYDOWN);
        if(verifyboard()==1) {
					finalizetime(); displayscore(x,y);
          messagebox(1,"You are winner!");
          return;
        }
      }
    }
  } while(event!=EVENT_QUIT && !keys[SDLK_ESCAPE]);
}
