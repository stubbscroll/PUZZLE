#include <stdio.h>
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
static int mx,my;               /*  maximal clue size */
static char difficulty[MAXSTR]; /*  string holding the difficulty */

#define UNFILLED      -100
#define EMPTY         -2
#define BLOCKED       -1
static int m[MAXS][MAXS];

/*  clues!  */
static int cluer[MAXS][MAXS]; /*  row clues */
static int cluec[MAXS][MAXS]; /*  column clues */
static int cluern[MAXS];      /*  number of clues for each row */
static int cluecn[MAXS];      /*  number of clues for each column */

static char touched[MAXS][MAXS];  /*  1 if cell is changed and need to be redrawn */

static int stv[MAXS][MAXS];    /*  colour for empty */
static int sth[MAXS][MAXS];    /*  colour for empty */

/*  move queue for hint system */
#define MAXMQ MAXS*MAXS*3
static int mq[MAXMQ];
static int mqs,mqe;

static void convline(char *s,int clue[MAXS],int *cluen) {
  int i=0,v;
  while(1) {
    for(v=0;isdigit(s[i]);i++) v=v*10+s[i]-48;
    clue[(*cluen)++]=v;
    if(!s[i] || s[i]=='\n' || s[i]=='\r') break;
    if(s[i]==',' || s[i]==' ') i++;
    else error("faulty input");
  }
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
      convline(s,cluer[ln],&cluern[ln]);
      ln++;
      if(ln==y) ln=0,z++;
      break;
    case 4:
      convline(s,cluec[ln],&cluecn[ln]);
      ln++;
      break;
    }
  }
  fclose(f);
  startx=10,starty=30;
  mqs=mqe=0;
  mx=my=0;
  for(i=0;i<x;i++) if(my<cluecn[i]) my=cluecn[i];
  for(i=0;i<y;i++) if(mx<cluern[i]) mx=cluern[i];
  for(i=0;i<x;i++) for(j=0;j<y;j++) m[i][j]=UNFILLED,stv[i][j]=0,sth[i][j]=0;
}

#define CHECK_ILLEGAL -1
#define CHECK_ALMOSTOK -2
#define CHECK_UNFILLED 0
#define CHECK_OK 1

/*  simple check (in given order):
    there is a contiguous segment longer than any hint => ILLEGAL
    if number of black cells==clues, or tot-blank+unfilled==clues
      if hint pattern doesn't match => ILLEGAL
      if match => ALMOSTOK or OK, depending on presence of unfilled
    otherwise UNFILLED */
static int checkrow(int ix) {
  int longest=0,i,cur=0,unfilled=0,at=0,hintsum=0,blocked=0,empty=0;
  for(i=0;i<cluern[ix];i++) {
    if(longest<cluer[ix][i]) longest=cluer[ix][i];
    hintsum+=cluer[ix][i];
  }
  for(i=0;i<x;i++) if(m[i][ix]==UNFILLED) unfilled++,cur=0;
  else if(m[i][ix]==EMPTY) empty++,cur=0;
  else if(m[i][ix]==BLOCKED) {
    cur++;
    blocked++;
    if(cur>longest) return CHECK_ILLEGAL;
  }
  if(blocked>hintsum || empty>x-hintsum) return CHECK_ILLEGAL;
  if(blocked==hintsum && x-empty-unfilled==hintsum) {
    for(i=at=cur=0;i<=x;i++) if(i<x && m[i][ix]==BLOCKED) cur++;
    else if(cur) {
      if(cur!=cluer[ix][at++]) return CHECK_ILLEGAL;
      cur=0;
    }
    return unfilled?CHECK_ALMOSTOK:CHECK_OK;
  }
  return CHECK_UNFILLED;
}

/*  code duplication, blah */
static int checkcolumn(int ix) {
  int longest=0,i,cur=0,unfilled=0,at=0,hintsum=0,blocked=0,empty=0;
  for(i=0;i<cluecn[ix];i++) {
    if(longest<cluec[ix][i]) longest=cluec[ix][i];
    hintsum+=cluec[ix][i];
  }
  for(i=0;i<y;i++) if(m[ix][i]==UNFILLED) unfilled++,cur=0;
  else if(m[ix][i]==EMPTY) empty++,cur=0;
  else if(m[ix][i]==BLOCKED) {
    cur++;
    blocked++;
    if(cur>longest) return CHECK_ILLEGAL;
  }
  if(blocked>hintsum || empty>y-hintsum) return CHECK_ILLEGAL;
  if(blocked==hintsum && y-empty-unfilled==hintsum) {
    for(i=at=cur=0;i<=y;i++) if(i<y && m[ix][i]==BLOCKED) cur++;
    else if(cur) {
      if(cur!=cluec[ix][at++]) return CHECK_ILLEGAL;
      cur=0;
    }
    return unfilled?CHECK_ALMOSTOK:CHECK_OK;
  }
  return CHECK_UNFILLED;
}

/*  return 1 if solved, 0 if unsolved, -1 if the board is illegally filled in */
static int verifyboard() {
  int i,ok=1,res;
  /*  check for illegal state */
  for(i=0;i<y;i++) {
    res=checkrow(i);
    if(res==-1) return -1;
    if(res<1) ok=0;
  }
  for(i=0;i<x;i++) {
    res=checkcolumn(i);
    if(res==-1) return -1;
    if(res<1) ok=0;
  }
  return ok;
}

/*  dp[i][j]: at position i, having processed j clues */
static int deepcheckrow(int ix) {
  static int dp[MAXS][MAXS];
  int i,j,k;
  for(i=0;i<x;i++) for(j=0;j<=cluern[ix];j++) dp[i][j]=0;
  dp[0][0]=1;
  for(i=0;i<x;i++) for(j=0;j<=cluern[ix];j++) if(dp[i][j]) {
    if(j<cluern[ix] && i+cluer[ix][j]<=x) {
      /*  add a segment, if possible */
      for(k=0;k<cluer[ix][j];k++) if(m[i+k][ix]==EMPTY) break;
      if(k==cluer[ix][j] && i+k<x && m[i+k][ix]!=BLOCKED) dp[i+k+1][j+1]=1;
      if(k==cluer[ix][j] && i+k==x) dp[i+k][j+1]=1;
    }
    /* add a blank, if possible */ 
    if(m[i][ix]!=BLOCKED) dp[i+1][j]=1;
  }
  return dp[x][cluern[ix]]==1?0:-1;
}

static int deepcheckcolumn(int ix) {
  static int dp[MAXS][MAXS];
  int i,j,k;
  for(i=0;i<y;i++) for(j=0;j<=cluecn[ix];j++) dp[i][j]=0;
  dp[0][0]=1;
  for(i=0;i<y;i++) for(j=0;j<=cluecn[ix];j++) if(dp[i][j]) {
    if(j<cluecn[ix] && i+cluec[ix][j]<=y) {
      /*  add a segment, if possible */
      for(k=0;k<cluec[ix][j];k++) if(m[ix][i+k]==EMPTY) break;
      if(k==cluec[ix][j] && i+k<y && m[ix][i+k]!=BLOCKED) dp[i+k+1][j+1]=1;
      if(k==cluec[ix][j] && i+k==y) dp[i+k][j+1]=1;
    }
    /* add a blank, if possible */ 
    if(m[ix][i]!=BLOCKED) dp[i+1][j]=1;
  }
  return dp[y][cluecn[ix]]==1?0:-1;
}

static int deepverifyboard() {
  int res=verifyboard(),i;
  if(res) return res;
  /*  check rows */
  for(i=0;i<y;i++) if(deepcheckrow(i)<0) return -1;
  /*  check columns */
  for(i=0;i<x;i++) if(deepcheckcolumn(i)<0) return -1;
  return 0;
}

static void updatecell(int u,int v) {
  int col;
  if(m[u][v]==UNFILLED) drawsolidcell32(u+mx,v+my,unfilledcol);
  else if(m[u][v]==EMPTY) {
    col=blankcol;
    if(stv[u][v]==CHECK_ILLEGAL || sth[u][v]==CHECK_ILLEGAL) col=errorcol;
    else if(stv[u][v]==CHECK_OK || sth[u][v]==CHECK_OK) col=okcol;
    else if(stv[u][v]==CHECK_ALMOSTOK || sth[u][v]==CHECK_ALMOSTOK) col=almostokcol;
    drawsolidcell32(u+mx,v+my,col);
  } else if(m[u][v]==BLOCKED) drawsolidcell32(u+mx,v+my,filledcol);
  else error("wrong tile value");
}

static void drawclue(int u,int v,int val,Uint8 col) {
  int bx=startx+width*u,by=starty+height*v;
  drawrectangle32(bx,by,bx+width+thick-1,by+thick-1,col);
  drawrectangle32(bx,by+height,bx+width+thick-1,by+height+thick-1,col);
  drawrectangle32(bx,by,bx+thick-1,by+height+thick-1,col);
  drawrectangle32(bx+width,by,bx+width+thick-1,by+height+thick-1,col);
  drawnumbercell32(u,v,val,col,col,WHITE32);
}

static void drawgrid() {
  int i,j;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  clear32(WHITE32);
  updatescale(resx-startx,resy-starty,x+mx,y+my,thick);
  /*  draw clues, column */
  for(i=0;i<x;i++) for(j=0;j<cluecn[i];j++) drawclue(i+mx, my-cluecn[i]+j,cluec[i][j],BLACK32);
  /*  row */
  for(i=0;i<y;i++) for(j=0;j<cluern[i];j++) drawclue(mx-cluern[i]+j,i+my,cluer[i][j],BLACK32);
  if(thick) {
    for(i=my;i<=my+y;i++) for(j=0;j<thick;j++) drawhorizontalline32(startx+width*mx,startx+width*(x+mx)+thick-1,starty+i*height+j,BLACK32);
    for(i=mx;i<=mx+x;i++) drawrectangle32(startx+width*i,starty+height*my,startx+i*width+thick-1,starty+(my+y)*height+thick-1,BLACK32);
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
    sdlrefreshcell(i+mx,j+my);
    touched[i][j]=0;
  }
}

static void updatetoscreen(int cellx,int celly,int visible) {
  int resx=checkrow(celly),resy=checkcolumn(cellx),i;
  for(i=0;i<x;i++) sth[i][celly]=resx;
  for(i=0;i<y;i++) stv[cellx][i]=resy;
  if(visible) partialredraw();
}

static void touchcross(int cx,int cy) {
  int i;
  for(i=0;i<x;i++) touched[i][cy]=1;
  for(i=0;i<y;i++) touched[cx][i]=1;
}

/*  do move bookkeeping, including putting it on the stack */
static void domove(int cellx,int celly,int val) {
  if(val==m[cellx][celly]) error("logical error, tried to set cell to existing value");
  stackpush(cellx); stackpush(celly); stackpush(m[cellx][celly]);
  m[cellx][celly]=val;
  touchcross(cellx,celly);
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
  int cellx,celly,v=controlscheme_picross,up=0;
  getcell(event_mousex,event_mousey,&cellx,&celly);
  cellx-=mx; celly-=my;
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

static void undo(int visible) {
  if(!stackempty()) {
    int val=stackpop(),celly=stackpop(),cellx=stackpop();
    m[cellx][celly]=val;
    touchcross(cellx,celly);
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

/*  if clue is 0, then all cells in row/column become empty */
static int level1empty() {
  int i,j,ok=0;
  for(i=0;i<x;i++) if(cluecn[i]==1 && cluec[i][0]==0) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) addmovetoqueue(i,j,EMPTY),ok=1;
  for(i=0;i<y;i++) if(cluern[i]==1 && cluer[i][0]==0) for(j=0;j<x;j++) if(m[j][i]==UNFILLED) addmovetoqueue(j,i,EMPTY),ok=1;
  return ok;
}

/*  if there is just one combination (disregarding existing tiles), apply it */
static int level1forced() {
  int i,j,ok=0,c,d;
  for(i=0;i<x;i++) {
    for(c=cluecn[i]-1,j=0;j<cluecn[i];j++) c+=cluec[i][j];
    if(c==y) for(j=c=0;j<cluecn[i];j++) {
      d=cluec[i][j];
      while(d--) {
        if(m[i][c]==UNFILLED) addmovetoqueue(i,c,BLOCKED),ok=1;
        c++;
      }
      if(c<y && m[i][c]==UNFILLED) addmovetoqueue(i,c,EMPTY),ok=1;
      c++;
    }
  }
  for(i=0;i<y;i++) {
    for(c=cluern[i]-1,j=0;j<cluern[i];j++) c+=cluer[i][j];
    if(c==x) for(j=c=0;j<cluern[i];j++) {
      d=cluer[i][j];
      while(d--) {
        if(m[c][i]==UNFILLED) addmovetoqueue(c,i,BLOCKED),ok=1;
        c++;
      }
      if(c<x && m[c][i]==UNFILLED) addmovetoqueue(c,i,EMPTY),ok=1;
      c++;
    }
  }
  return ok;
}

static int level1hint() {
  if(level1empty()) return 1;
  if(level1forced()) return 1;
  return 0;
}

static int level2hint() {
  return 0;
}

static int level3hint() {
  return 0;
}

/*  store final results here, including no intersection */
#define NOINTERSECT 100
static int btrres[MAXS];
static int btrcont[MAXS];

/*  backtracking: try all ways */
/*  at:   position we're at
    left: how many blanks left
    clue: what clue we're at
    cn:   number of clues
    clues:pointer to clues
*/
static void level4btr(int at,int max,int left,int clue,int cn,int *clues) {
  /*  warning, stack haavy */
  int bak,bk[MAXS],i;
  /*  for each step, either place blank (use "left") or place a filled interval */
  if(at>=max) {
    /*  apply */
    for(i=0;i<max;i++) {
      if(btrres[i]==UNFILLED) btrres[i]=btrcont[i];
      else if(btrres[i]!=btrcont[i]) btrres[i]=NOINTERSECT;
    }
    return;
  }
  if(left) {
    /*  place extra blank */
    if(btrcont[at]!=BLOCKED) {
      bak=btrcont[at];
      btrcont[at]=EMPTY;
      level4btr(at+1,max,left-1,clue,cn,clues);
      btrcont[at]=bak;
    }
  }
  if(clue==cn) return;  /*  no more clues */
  /*  place segment! check if it fits */
  for(i=0;i<clues[clue];i++) if(btrcont[i+at]==EMPTY) return;
  /*  unless we're at end of segment, check if following cell isn't wall */
  if(at+clues[clue]<max && btrcont[at+clues[clue]]==BLOCKED) return;
  /*  backup */
  for(i=0;i<=clues[clue];i++) bk[i]=btrcont[i+at];
  for(i=0;i<clues[clue];i++) if(btrcont[i+at]==UNFILLED) btrcont[i+at]=BLOCKED;
  if(at+i<max && btrcont[i+at]==UNFILLED) btrcont[i+at]=EMPTY;
  level4btr(at+1+clues[clue],max,left,clue+1,cn,clues);
  /*  restore */
  for(i=0;i<=clues[clue];i++) btrcont[i+at]=bk[i];
}

/*  try all combinations for a row or column */
static int level4tryall() {
  int i,j,ok=0,c;
  /*  try all possible ways to fill in a row */
  for(i=0;i<y;i++) {
    /*  only process a row if it has unfilled squares */
    for(j=0;j<x;j++) if(m[j][i]==UNFILLED) goto process;
    continue;
  process:
    /*  copy contents to processing array */
    for(j=0;j<x;j++) btrcont[j]=btrres[j]=m[j][i];
    /*  count leeway */
    for(c=x+1,j=0;j<cluern[i];j++) c-=cluer[i][j]+1;
    /*  run backtracking */
    level4btr(0,x,c,0,cluern[i],cluer[i]);
    /*  apply changes */
    for(j=0;j<x;j++) if(m[j][i]==UNFILLED && btrres[j]!=NOINTERSECT && btrres[j]!=UNFILLED) {
      addmovetoqueue(j,i,btrres[j]);
      ok=1;
    }
    if(ok) return 1;
  }
  /*  try all possible ways to fill in a column */
  for(i=0;i<x;i++) {
    /*  only process a row if it has unfilled squares */
    for(j=0;j<y;j++) if(m[i][j]==UNFILLED) goto process2;
    continue;
  process2:
    /*  copy contents to processing array */
    for(j=0;j<y;j++) btrcont[j]=btrres[j]=m[i][j];
    /*  count leeway */
    for(c=y+1,j=0;j<cluecn[i];j++) c-=cluec[i][j]+1;
    /*  run backtracking */
    level4btr(0,y,c,0,cluecn[i],cluec[i]);
    /*  apply changes */
    for(j=0;j<y;j++) if(m[i][j]==UNFILLED && btrres[j]!=NOINTERSECT && btrres[j]!=UNFILLED) {
      addmovetoqueue(i,j,btrres[j]);
      ok=1;
    }
    if(ok) return 1;
  }
  return 0;
}

static int level4hint() {
  if(level4tryall()) return 1;
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
    r=deepverifyboard();
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

static int level5hint() {
  if(level5contradiction(4)) return 1;
  return 0;
}

static int hint() {
  if(deepverifyboard()<0) return -1;
  if(level1hint()) return 1;
  if(level2hint()) return 1;
  if(level3hint()) return 1;
  if(level4hint()) return 1;
  if(level5hint()) return 1;
  return 0;
}

static void processkeydown(int key) {
  int res;
  if(key==undokey) undo(1);
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
      if(deepverifyboard()<1) messagebox("Sorry, no more moves found.");
    } else if(!res) messagebox("Sorry, no moves found.");
    else messagebox("Sorry, hint will not work on an illegal board.");
  }
}

static void autosolver(char *s) {
  int res;
  double start=gettime(),end;
  logprintf("%s: ",s);
  while(hint()>0) executemovequeue();
  res=deepverifyboard();
  end=gettime()-start;
  if(end<0) end=0;
  logprintf("[%.3fs] ",end);
  if(res==-1) logprintf("Solver reached illegal state!\n");
  else if(!res) logprintf("Not solved\n");
  else logprintf("Solved!\n");
}

void picross(char *path,int solve) {
  int event;
  loadpuzzle(path);
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
