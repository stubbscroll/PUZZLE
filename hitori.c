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

/*  maximal number in a puzzle */
#define MAXNUM 128

#define UNFILLED    -100
#define EMPTY       -2
#define BLOCKED     -1
static int m[MAXS][MAXS];
static int mn[MAXS][MAXS];      /*  map containing numbers */
static int maxnum;              /*  largest number occurring in puzzle instance */

/*  strow[][], stcol[][] are guaranteed to be maintained */
static int strow[MAXS][MAXNUM]; /*  [i][j] number of unmarked numbers j in row i */
static int stcol[MAXS][MAXNUM]; /*  [i][j] number of unmarked numbers j in column i */

static char touched[MAXS][MAXS];  /*  1 if cell is changed and need to be redrawn */

/*  cells in the same row/column having the same number */
static int samex[MAXS][MAXS][MAXS*2]; /*  coordinate of other cell with same number */
static int samey[MAXS][MAXS][MAXS*2]; 
static int samen[MAXS][MAXS];         /*  number of other cells with same number */

/*  move queue for hint system */
#define MAXMQ MAXS*MAXS*3
static int mq[MAXMQ];
static int mqs,mqe;

static int autosolve;

static int convchar(char *s,int *i) {
  char c=s[(*i)++];
  if(!c) error("string in level definition ended prematurely.");
  else if(c>='0' && c<='9') return c-48;
  else if(c>='a' && c<='z') return c-'a'+10;
  else if(c>='A' && c<='Z') return c-'A'+36;
  error("invalid character %c in level definition.",c);
  return 0;
}

static void loadpuzzle(char *path) {
  static char s[MAXSTR];
  FILE *f=fopen(path,"r");
  int z=0,ln=0,i,j,k;
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
  maxnum=-1;
  memset(stcol,0,sizeof(stcol));
  memset(strow,0,sizeof(strow));
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    m[i][j]=UNFILLED;
    if(maxnum<mn[i][j]) maxnum=mn[i][j];
    strow[j][mn[i][j]]++;
    stcol[i][mn[i][j]]++;
    samen[i][j]=0;
    /*  row */
    for(k=0;k<x;k++) if(k!=i && mn[k][j]==mn[i][j]) {
      samex[i][j][samen[i][j]]=k;
      samey[i][j][samen[i][j]++]=j;
    }
    /*  column */
    for(k=0;k<y;k++) if(k!=j && mn[i][k]==mn[i][j]) {
      samex[i][j][samen[i][j]]=i;
      samey[i][j][samen[i][j]++]=k;
    }
  }
}

static int adjacentwall(int cx,int cy) {
  int i,x2,y2;
  for(i=0;i<4;i++) {
    x2=cx+dx[i]; y2=cy+dy[i];
    if(x2<0 || y2<0 || x2>=x || y2>=y) continue;
    if(m[x2][y2]==BLOCKED) return 1;
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

/*  generic bfs!
    type  0:  search for empty & unfilled
    returns:  number of cells examined  */
static int genericbfs(int sx,int sy,int type) {
  int tot=1,i,cx,cy,x2,y2;
  if(visit[sx][sy]) return 0;
  if(type==0 && m[sx][sy]>=BLOCKED) return 0;
  q[qe++]=sx; q[qe++]=sy;
  visit[sx][sy]=1;
  while(qs<qe) {
    cx=q[qs++]; cy=q[qs++];
    for(i=0;i<4;i++) {
      x2=cx+dx[i],y2=cy+dy[i];
      if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2]) continue;
      if(type==0 && m[x2][y2]>=BLOCKED) continue;
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

/*  return 1 if solved, 0 if unsolved, -1 if the board is illegally filled in */
static int verifyboard() {
  int i,j,solved=1,emptyreg=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    /*  check if two walls are adjacent: illegal */
    if(m[i][j]==BLOCKED && adjacentwall(i,j)) return -1;
    /*  check if a row or column has two of one number: unsolved */
    if(stcol[i][mn[i][j]]>1 || strow[j][mn[i][j]]>1) solved=0;
    /*  simply not done filling in */
    if(m[i][j]==UNFILLED) solved=0;
  }
  /*  check if empty area is disconnected: illegal */
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && m[i][j]<BLOCKED) {
    if(++emptyreg>1) goto out;
    genericbfs(i,j,0);
  }
out:
  cleanupbfs();
  if(emptyreg!=1) return -1;
  return solved;
}

/*  colour scheme:
    an empty cell containing a number which is unique for its row and column: okcol
    other empty cells: emptycol
    adjacent walls: darkererrorcol
    walls: blockedcol */
static void updatecell(int u,int v,int override) {
  Uint32 col=0,bk=0;
  int num=mn[u][v];
  if(m[u][v]==UNFILLED) bk=strow[v][num]>1||stcol[u][num]>1?mustprocesscol:unfilledcol,col=BLACK32;
  else if(m[u][v]==EMPTY) bk=strow[v][num]>1||stcol[u][num]>1?blankcol:okcol,col=BLACK32;
  else if(m[u][v]==BLOCKED) bk=adjacentwall(u,v)?darkererrorcol:filledcol,col=WHITE32;
  if(m[u][v]<BLOCKED && override>-1) bk=override;
  drawnumbercell32(u,v,mn[u][v],col,col,bk);
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
  for(i=0;i<x;i++) for(j=0;j<y;j++) updatecell(i,j,-1);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
}

static void changest(int cellx,int celly,int inc) {
  int i,n,cx,cy;
  n=mn[cellx][celly];
  strow[celly][n]+=inc;
  stcol[cellx][n]+=inc;
  for(i=0;i<samen[cellx][celly];i++) {
    cx=samex[cellx][celly][i];
    cy=samey[cellx][celly][i];
    if(strow[cy][n]==(inc>0?2:1) || stcol[cx][n]==(inc>0?2:1)) touched[cx][cy]=1;
  }
}

static void applymove(int cellx,int celly,int val) {
  int old=m[cellx][celly],dir,cx,cy;
  m[cellx][celly]=val;
  touched[cellx][celly]=1;
  if(old==BLOCKED) changest(cellx,celly,1);
  else if(val==BLOCKED) changest(cellx,celly,-1);
  if(old==BLOCKED || val==BLOCKED) for(dir=0;dir<4;dir++) {
    cx=cellx+dx[dir]; cy=celly+dy[dir];
    if(cx<0 || cy<0 || cx>=x || cy>=y || m[cx][cy]!=BLOCKED) continue;
    touched[cx][cy]=1;
  }
}

static void domove(int cellx,int celly,int val) {
  if(val==m[cellx][celly]) error("logical error, tried to set cell to existing value");
  stackpush(cellx); stackpush(celly); stackpush(m[cellx][celly]);
  applymove(cellx,celly,val);
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

static void updatetoscreen(int cellx,int celly,int visible) {
  if(visible) partialredraw();
}

static void undo(int visible) {
  if(!stackempty()) {
    int val=stackpop(),celly=stackpop(),cellx=stackpop();
    applymove(cellx,celly,val);
    updatetoscreen(cellx,celly,visible);
  }
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
  int cellx,celly,v=controlscheme_hitori,up=0;
  getcell(event_mousex,event_mousey,&cellx,&celly);
  if(cellx<0 || celly<0 || cellx>=x || celly>=y) return;
  if(event_mousebutton==SDL_BUTTON_LEFT) {
    if(!v) {
      domove(cellx,celly,togglecell(m[cellx][celly])); up=1;
    } else if(v==1 && m[cellx][celly]!=BLOCKED) {
      domove(cellx,celly,BLOCKED); up=1;
    } else if(v==2 && m[cellx][celly]!=EMPTY) {
      domove(cellx,celly,EMPTY); up=1;
    }
  } else if(event_mousebutton==SDL_BUTTON_RIGHT) {
    if(!v && m[cellx][celly]!=UNFILLED) {
      domove(cellx,celly,UNFILLED); up=1;
    } else if(v==1 && m[cellx][celly]!=EMPTY) {
      domove(cellx,celly,EMPTY); up=1;
    } else if(v==2 && m[cellx][celly]!=BLOCKED) {
      domove(cellx,celly,BLOCKED); up=1;
    }
  } else if(event_mousebutton==SDL_BUTTON_MIDDLE) {
    if(v && m[cellx][celly]!=UNFILLED) {
      domove(cellx,celly,UNFILLED); up=1;
    }
  }
  if(up) updatetoscreen(cellx,celly,1);
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

/*  the cell's value is unique in its row/column: no need whatsoever for it to
    be blocked */
static int level1forcedempty() {
  int i,j,ok=0,n;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
    n=mn[i][j];
    if(stcol[i][n]>1 || strow[j][n]>1) continue;
    addmovetoqueue(i,j,EMPTY);
    ok=1;
  }
  return ok;
}

/*  if there are three equal numbers in a row, the two at the ends must be blocked */
static int level1threeinarow() {
  int i,j,ok=0,n;
  for(i=0;i<x;i++) for(j=0;j<y;j++) {
    n=mn[i][j];
    if(i<x-2 && mn[i+1][j]==n && mn[i+2][j]==n) {
      if(m[i][j]==UNFILLED) addmovetoqueue(i,j,BLOCKED),ok=1;
      if(m[i+2][j]==UNFILLED) addmovetoqueue(i+2,j,BLOCKED),ok=1;
    }
    if(j<y-2 && mn[i][j+1]==n && mn[i][j+2]==n) {
      if(m[i][j]==UNFILLED) addmovetoqueue(i,j,BLOCKED),ok=1;
      if(m[i][j+2]==UNFILLED) addmovetoqueue(i,j+2,BLOCKED),ok=1;
    }
  }
  return ok;
}

/*  surround wall with empty */
static int level1emptynearblocked() {
  int i,j,ok=0,k,cx,cy;
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

static int level1hint() {
  if(level1forcedempty()) return 1;
  if(level1threeinarow()) return 1;
  if(level1emptynearblocked()) return 1;
  return 0;
}

/*  if a row/column has two or more of a number and one is filled in with empty,
    the rest must be wall */
static int level2onepossibility() {
  int i,j,k,cx,cy,ok=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==EMPTY) {
    for(k=0;k<samen[i][j];k++) {
      cx=samex[i][j][k];
      cy=samey[i][j][k];
      if(m[cx][cy]==UNFILLED) addmovetoqueue(cx,cy,BLOCKED),ok=1;
    }
    if(ok) return 1;
  }
  return 0;
}

static int level2hint() {
  if(level2onepossibility()) return 1;
  return 0;
}

static int level3hint() {
  return 0;
}

/*  if placing a wall in a cell causes the empty area to be disconnected, then
    that cell must be empty */
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
  static int i=0,j=0;
  int z=x*y,r,k,l,oldsp=getstackpos();
	if(i>=x) i=0; if(j>=y) j=0;
  while(z--) {
    if(m[i][j]==UNFILLED) {
      /*  assume blocked */
      domove(i,j,BLOCKED);
      updatetoscreen(i,j,0);
      r=dogreedy(lev);
      if(r<0) {
        /*  contradiction! */
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
      if(r<0) {
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

static int level5hint() {
  if(level5contradiction(3)) return 1;
  if(level5contradiction(4)) return 1;
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

#define COLSIZE 6
static Uint32 colarray[]={
  0xFFDBFF, 0xFFFF00, 0xFFFFAA, 0xB6FF00, 0x92FFAA, 0xFFB600};

static int forcefullredraw;

static void drawverify() {
  int i,j,col=0,k,qs2;
  if(forcefullredraw) drawgrid();
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && m[i][j]<BLOCKED) {
    qs2=qs;
    genericbfs(i,j,0);
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
  if(key==undokey) undo(1);
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

void hitori(char *path,int solve) {
  int event;
  autosolve=solve;
  loadpuzzle(path);
  initbfs();
  drawgrid();
  if(autosolve) { autosolver(path); return; }
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
