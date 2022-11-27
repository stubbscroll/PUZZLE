#include <stdio.h>
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

#define NOINTERSECT -99
#define NOVALUE -101

#define UNFILLED    -100
#define EMPTY       -2
#define BLOCKED			-1
/*  internal format:
    -100: unfilled cell
    -2:   empty cell
    -1:   blocked
    0-:   numbered cell
*/
static int m[MAXS][MAXS];
static char touched[MAXS][MAXS]; /* 1 if cell needs to be redrawn */

/*  move queue for hint system */
#define MAXMQ MAXS*MAXS*3
static int mq[MAXMQ];
static int mqs,mqe;

static int convchar(char *s,int *i) {
  char c=s[(*i)++];
  if(!c) error("string in level definition ended prematurely.");
  else if(c=='.') return UNFILLED;
	else if(c>='0' && c<='9') return c-48;
	else if(c>='a' && c<='z') return c-'a'+10;
	else if(c>='A' && c<='Z') return c-'A'+36;
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
      sscanf(s,"%d %d",&x,&y);
      z++;
      break;
    case 3:
      for(i=j=0;j<x;j++) m[j][ln]=convchar(s,&i);
      ln++;
    }
  } else if(!gameinfo[0]) strcpy(gameinfo,s+2);
  fclose(f);
  startx=10,starty=(int)(font->height*2.5);
  mqs=mqe=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) touched[i][j]=0;
}

/* check one direction. return un=1 if unfilled was encountered, 0
   otherwise.
   type 0: stop at blocked
	 type 1: stop at blocked/unfilled
	 return number of cells traversed, don't count start cell */
static int countdir(int u,int v,int d,int type,int *un) {
	int count=0;
	*un=0;
	u+=dx[d]; v+=dy[d];
	while(u>=0 && v>=0 && u<x && v<y) {
		if((type==0 || type==1) && m[u][v]==BLOCKED) break;
		if(type==1 && m[u][v]==UNFILLED) break;
		if(m[u][v]==UNFILLED) *un=1;
		count++;
		u+=dx[d]; v+=dy[d];
	}
	return count;
}

static int count(int u,int v) {
	int seen=1,d,dummy;
	for(d=0;d<4;d++) seen+=countdir(u,v,d,0,&dummy);
	return seen;
}

static int hasneighbouringblocked(int u,int v) {
	int d,x2,y2;
	for(d=0;d<4;d++) {
		x2=u+dx[d]; y2=v+dy[d];
		if(x2<0 || y2<0 || x2>=x || y2>=y) continue;
		if(m[x2][y2]==BLOCKED) return 1;
	}
	return 0;
}

static void updatecell(int u,int v,int override) {
	int seen,dummy,d,un;
	Uint32 col=0;
	if(m[u][v]==UNFILLED) drawsolidcell32(u,v,override>-1?override:unfilledcol);
	else if(m[u][v]==EMPTY) drawsolidcell32(u,v,override>-1?override:blankcol);
	else if(m[u][v]==BLOCKED) {
		drawsolidcell32(u,v,hasneighbouringblocked(u,v)?darkererrorcol:filledcol);
	} else if(m[u][v]>-1) {
		/* white - seen cells > number
			 yellow - seen cells == number, unfilled 
			 green - seen cells == number, no unfilled
		   red   - seen cells < number OR seen white cells > number */
		for(seen=1,un=d=0;d<4;d++) {
			seen+=countdir(u,v,d,0,&dummy);
			if(dummy) un=1;
		}
		seen=count(u,v);
		if(seen>m[u][v]) col=blankcol;
		else if(seen==m[u][v] && un) col=almostokcol;
		else if(seen==m[u][v]) col=okcol;
		else col=errorcol;
		for(d=0,seen=1;d<4;d++) seen+=countdir(u,v,d,1,&dummy);
		if(seen>m[u][v]) col=errorcol;
		else if(seen==m[u][v] && col==blankcol) col=almostokcol;
		drawnumbercell32(u,v,m[u][v],filledcol,filledcol,override>-1?override:col);
	}
}

static void drawgrid() {
  int i,j;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  clear32(WHITE32);
	/* added +1 to allow for "mines left" info */
  updatescale(resx-startx,resy-starty,x,y+2,thick);
  if(thick) {
    for(i=0;i<=y;i++) for(j=0;j<thick;j++) drawhorizontalline32(startx,startx+width*x+thick-1,starty+i*height+j,BLACK32);
    for(i=0;i<=x;i++) drawrectangle32(startx+width*i,starty,startx+i*width+thick-1,starty+y*height+thick-1,BLACK32);
  }
  for(i=0;i<x;i++) for(j=0;j<y;j++) updatecell(i,j,-1);
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
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

static void updatetoscreen(int visible) {
  if(visible) partialredraw();
}

static void applymove(int cellx,int celly,int val,int visible) {
	int d,x2,y2;
	m[cellx][celly]=val;
	if(visible) {
		touched[cellx][celly]=1;
		/* scan in all directions (until a wall is encountered) and touch
		   all numbered cells */
		for(d=0;d<4;d++) {
			x2=cellx+dx[d]; y2=celly+dy[d];
			if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]==BLOCKED)
				touched[x2][y2]=1;
			while(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]!=BLOCKED) {
				if(m[x2][y2]>-1) touched[x2][y2]=1;
				x2+=dx[d]; y2+=dy[d];
			}
		}
	}
}

static void undo(int visible) {
  if(!stackempty()) {
    int val=stackpop(),celly=stackpop(),cellx=stackpop();
    applymove(cellx,celly,val,visible);
    updatetoscreen(visible);
  }
}

/* do move bookkeeping, including putting it on the stack */
static void domove(int cellx,int celly,int val,int visible) {
  if(val==m[cellx][celly]) error("logical error, tried to set cell to existing value");
  stackpush(cellx); stackpush(celly); stackpush(m[cellx][celly]);
  applymove(cellx,celly,val,visible);
}

/* bfs stuff */
static uchar visit[MAXS][MAXS];
static int qs,qe,q[MAXS*MAXS*2];

/* bfs! type 0: search through unfilled,empty,number
        type 1: search through empty, number */
static void genericbfs(int sx,int sy,int type) {
	int cx,cy,d,x2,y2;
	if(type==0 && m[sx][sy]==BLOCKED) return;
	else if(type==1 && (m[sx][sy]==UNFILLED || m[sx][sy]==BLOCKED)) return;
	if(visit[sx][sy]) return;
	visit[sx][sy]=1;
	q[qe++]=sx; q[qe++]=sy;
	while(qs<qe) {
		cx=q[qs++]; cy=q[qs++];
		for(d=0;d<4;d++) {
			x2=cx+dx[d]; y2=cy+dy[d];
			if(x2<0 || y2<0 || x2>=x || y2>=y || visit[x2][y2]) continue;
			if(type==0 && m[x2][y2]==BLOCKED) continue;
			else if(type==1 && (m[x2][y2]==BLOCKED || m[x2][y2]==UNFILLED)) continue;
			visit[x2][y2]=1;
			q[qe++]=x2; q[qe++]=y2;
		}
	}
}

/* clean up the data structures, clear visited and reset queue pos
   (must be done after a series of bfs-es) */
static void cleanupbfs() {
  while(qe) visit[q[qe-2]][q[qe-1]]=0,qe-=2;
  qs=0;
}

static int verifyboard() {
	int i,j,incomplete=0,seen,k,dummy;
	/* check if there are numbered cells with too few seen cells */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>-1) {
		seen=count(i,j);
		if(seen<m[i][j]) return -1;
		if(seen>m[i][j]) incomplete=1;
		/* check if too many cells (up to neared blocked/unfilled) are seen */
		for(seen=1,k=0;k<4;k++) seen+=countdir(i,j,k,1,&dummy);
		if(seen>m[i][j]) return -1;
	} else if(m[i][j]==UNFILLED) incomplete=1;
	/* check for neighbouring blocked */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==BLOCKED && hasneighbouringblocked(i,j)) return -1;
	/* check if the white region is disconnected (bfs that cannot
	   pass blocked) */
	for(seen=i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]!=BLOCKED && !visit[i][j]) {
		if(seen) {
			cleanupbfs();
			return -1;
		}
		seen++;
		genericbfs(i,j,0);
	}
	cleanupbfs();
	return 1-incomplete;
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
  domove(mq[mqs],mq[mqs+1],mq[mqs+2],visible);
  updatetoscreen(visible);
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

static int level1emptynearblocked() {
	int i,j,ok=0;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED && hasneighbouringblocked(i,j))
		addmovetoqueue(i,j,EMPTY),ok=1;
	return ok;
}

static int level1hint() {
	if(level1emptynearblocked()) return 1;
	return 0;
}

static int level2placeblockedclose() {
	int i,j,count,d,x2,y2,x3[4],y3[4],ok=0;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>-1) {
		count=1;
		for(d=0;d<4;d++) {
			x2=i+dx[d]; y2=j+dy[d];
			x3[d]=-1;
			while(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]!=BLOCKED) {
				if(m[x2][y2]==UNFILLED && !hasneighbouringblocked(x2,y2)) {
					x3[d]=x2; y3[d]=y2;
					break;
				}
				x2+=dx[d]; y2+=dy[d];
				count++;
				if(count>m[i][j]) goto fail;
			}
		}
		if(count==m[i][j]) for(d=0;d<4;d++) if(x3[d]>-1)
			addmovetoqueue(x3[d],y3[d],BLOCKED),ok=1;
		if(ok) return 1;
	fail:;
	}
	return 0;
}

static int level2placeallempty() {
	int i,j,count,d,x2,y2,found;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>-1) {
		count=1; found=0;
		for(d=0;d<4;d++) {
			x2=i+dx[d]; y2=j+dy[d];
			while(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]!=BLOCKED) {
				if(m[x2][y2]==UNFILLED) found=1;
				x2+=dx[d]; y2+=dy[d];
				count++;
			}
		}
		if(count!=m[i][j] || !found) continue;
		for(d=0;d<4;d++) {
			x2=i+dx[d]; y2=j+dy[d];
			while(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]!=BLOCKED) {
				if(m[x2][y2]==UNFILLED) addmovetoqueue(x2,y2,EMPTY);
				x2+=dx[d]; y2+=dy[d];
			}
		}
		return 1;
	}
	return 0;
}

/* if it's only possible to expand in one direction, do it */
static int level2forceddir() {
	int i,j,d,f=0,seen,cur,un,left,x2,y2;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>-1) {
		seen=1; left=0;
		for(d=0;d<4;d++) {
			cur=countdir(i,j,d,0,&un);
			if(un) left++,f=d;
			else seen+=cur;
		}
		if(left==1) {
			x2=i+dx[f]; y2=j+dy[f];
			while(seen++<m[i][j]) {
				if(m[x2][y2]==UNFILLED) addmovetoqueue(x2,y2,EMPTY);
				x2+=dx[f]; y2+=dy[f];
			}
			if(x2>=0 && x2<x && y2>=0 && y2<y) addmovetoqueue(x2,y2,BLOCKED);
			return 1;
		}
	}
	return 0;
}

static int level2hint() {
	if(level2placeblockedclose()) return 1;
	if(level2placeallempty()) return 1;
	if(level2forceddir()) return 1;
	return 0;
}

static int level3contradiction() {
	static int i=0,j=0;
	int z=x*y,r;
	if(i>=x) i=0;
	if(j>=y) j=0;
	while(z--) {
		if(m[i][j]!=UNFILLED) goto next;
		domove(i,j,BLOCKED,0);
		r=verifyboard();
		undo(0);
		if(r<0) {
			addmovetoqueue(i,j,EMPTY);
			return 1;
		}
		domove(i,j,EMPTY,0);
		r=verifyboard();
		undo(0);
		if(r<0) {
			addmovetoqueue(i,j,BLOCKED);
			return 1;
		}
	next:
		i++;
		if(i==x) {
			i=0;
			j++;
			if(j==y) j=0;
		}
	}
	return 0;
}

static int level3hint() {
	if(level3contradiction()) return 1;
	return 0;
}

/* try all ways of extending a number and take intersection */
static int level4tryall() {
	static int i=0,j=0,try[4][MAXS],tryn[4],has3[MAXS];
	int z=x*y,len[4],max[4],min[4],k,l,x2,y2,oldsp=getstackpos(),w;
	int can[4],r,ok=0;
	if(i>=x) i=0;
	if(j>=y) j=0;
	while(z--) {
		if(m[i][j]<0) goto next;
		/* must be at least 2 unfinished directions */
		for(l=k=0;k<4;k++) {
			countdir(i,j,k,0,&can[k]);
			if(can[k]) l++;
		}
		if(l<2) goto next;
		for(k=0;k<4;k++) max[k]=len[k]=0,min[k]=MAXS;
		memset(has3,-1,(x>y?x+1:y+1)*sizeof(int));
		for(k=0;k<4;k++) {
			tryn[k]=0;
			x2=i+dx[k]; y2=j+dy[k]; l=1;
			while(x2>=0 && y2>=0 && x2<x && y2<y) {
				if(m[x2][y2]==UNFILLED) try[k][tryn[k]++]=l;
				if(m[x2][y2]==BLOCKED) break;
				l++;
				if(l>m[i][j]) goto nextdir;
				x2+=dx[k]; y2+=dy[k];
			}
			try[k][tryn[k]++]=l;
		nextdir:;
		}
		for(k=0;k<tryn[3];k++) has3[try[3][k]]=k;
		while(1) {
			/* calculate len[3] and check if position is legal */
			len[3]=m[i][j]-try[0][len[0]]-try[1][len[1]]-try[2][len[2]]+3;
			if(len[3]<1 || has3[len[3]]<0) goto nextcomb;
			len[3]=has3[len[3]];
			/* apply */
			for(k=0;k<4;k++) if(can[k]) {
				x2=i+dx[k]; y2=j+dy[k];
				w=try[k][len[k]];
				for(l=1;l<w;l++) {
					if(m[x2][y2]==UNFILLED) domove(x2,y2,EMPTY,0);
					x2+=dx[k]; y2+=dy[k];
				}
				if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]==UNFILLED)
					domove(x2,y2,BLOCKED,0);
			}
			r=verifyboard();
			while(getstackpos()>oldsp) undo(0);
			if(r>-1) for(k=0;k<4;k++) {
				l=try[k][len[k]];
				if(min[k]>l) min[k]=l;
				if(max[k]<l) max[k]=l;
			}
			/* increase */
		nextcomb:
			/* check if we already lost */
			for(k=l=0;k<4;k++) if(min[k]<max[k] && min[k]==try[k][0]) l++;
			if(l>1) goto next;
			k=2;
		again:
			if(k<0) break;
			len[k]++;
			if(len[k]==tryn[k]) {
				len[k]=0;
				k--;
				goto again;
			}
		}
		/* intersection */
		for(k=0;k<4;k++) if(can[k]) {
			x2=i+dx[k]; y2=j+dy[k];
			for(l=1;l<min[k];l++) {
				if(m[x2][y2]==UNFILLED) addmovetoqueue(x2,y2,EMPTY),ok=1;
				x2+=dx[k]; y2+=dy[k];
			}
			if(max[k]==min[k] && x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]==UNFILLED)
				addmovetoqueue(x2,y2,BLOCKED),ok=1;
		}
		if(ok) return 1;
	next:
		i++;
		if(i==x) {
			i=0;
			j++;
			if(j==y) j=0;
		}
	}
	return 0;
}

static int level4hint() {
	if(level4tryall()) return 1;
	return 0;
}

static int lev5m[MAXS][MAXS];

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

/* unfortunately, this routine is almost identical to the level4 version */
static int level5tryall() {
	static int i=0,j=0,try[4][MAXS],tryn[4],has3[MAXS];
	int z=x*y,len[4],max[4],min[4],k,l,x2,y2,oldsp=getstackpos(),w;
	int can[4],ok=0;
	if(i>=x) i=0;
	if(j>=y) j=0;
	while(z--) {
		if(m[i][j]<0) goto next;
		/* must be at least 2 unfinished directions */
		for(l=k=0;k<4;k++) {
			countdir(i,j,k,0,&can[k]);
			if(can[k]) l++;
		}
		if(l<2) goto next;
		for(k=0;k<x;k++) for(l=0;l<y;l++) lev5m[k][l]=NOVALUE;
		for(k=0;k<4;k++) max[k]=len[k]=0,min[k]=MAXS;
		memset(has3,-1,(x>y?x+1:y+1)*sizeof(int));
		for(k=0;k<4;k++) {
			tryn[k]=0;
			x2=i+dx[k]; y2=j+dy[k]; l=1;
			while(x2>=0 && y2>=0 && x2<x && y2<y) {
				if(m[x2][y2]==UNFILLED) try[k][tryn[k]++]=l;
				if(m[x2][y2]==BLOCKED) break;
				l++;
				if(l>m[i][j]) goto nextdir;
				x2+=dx[k]; y2+=dy[k];
			}
			try[k][tryn[k]++]=l;
		nextdir:;
		}
		for(k=0;k<tryn[3];k++) has3[try[3][k]]=k;
		while(1) {
			/* calculate len[3] and check if position is legal */
			len[3]=m[i][j]-try[0][len[0]]-try[1][len[1]]-try[2][len[2]]+3;
			if(len[3]<1 || has3[len[3]]<0) goto nextcomb;
			len[3]=has3[len[3]];
			/* apply */
			for(k=0;k<4;k++) if(can[k]) {
				x2=i+dx[k]; y2=j+dy[k];
				w=try[k][len[k]];
				for(l=1;l<w;l++) {
					if(m[x2][y2]==UNFILLED) domove(x2,y2,EMPTY,0);
					x2+=dx[k]; y2+=dy[k];
				}
				if(x2>=0 && y2>=0 && x2<x && y2<y && m[x2][y2]==UNFILLED)
					domove(x2,y2,BLOCKED,0);
			}
			/* greedy! */
			if(dogreedy(4)>-1) for(k=0;k<x;k++) for(l=0;l<y;l++) {
				if(lev5m[k][l]==NOVALUE && m[k][l]!=UNFILLED) lev5m[k][l]=m[k][l];
				else if(lev5m[k][l]!=m[k][l]) lev5m[k][l]=NOINTERSECT;
			}
			while(getstackpos()>oldsp) undo(0);
			/* increase */
		nextcomb:
			/* check if we already lost */
			for(k=l=0;k<4;k++) if(min[k]<max[k] && min[k]==try[k][0]) l++;
			if(l>1) goto next;
			k=2;
		again:
			if(k<0) break;
			len[k]++;
			if(len[k]==tryn[k]) {
				len[k]=0;
				k--;
				goto again;
			}
		}
		/* intersection */
		for(k=0;k<x;k++) for(l=0;l<y;l++) {
			if(lev5m[k][l]!=NOVALUE && lev5m[k][l]!=NOINTERSECT && lev5m[k][l]!=m[k][l])
				addmovetoqueue(k,l,lev5m[k][l]),ok=1;
		}
		if(ok) return 1;
	next:
		i++;
		if(i==x) {
			i=0;
			j++;
			if(j==y) j=0;
		}
	}
	return 0;
}

static int level5hint() {
	if(level5tryall()) return 1;
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
  int cellx,celly,v=controlscheme_kuromasu,up=0;
  getcell(event_mousex,event_mousey,&cellx,&celly);
  if(cellx<0 || celly<0 || cellx>=x || celly>=y || m[cellx][celly]>-1) return;
  if(event_mousebutton==SDL_BUTTON_LEFT) {
    if(!v) {
      domove(cellx,celly,togglecell(m[cellx][celly]),1); up=1;
    } else if(v==1 && m[cellx][celly]!=BLOCKED) {
      domove(cellx,celly,BLOCKED,1); up=1;
    } else if(v==2 && m[cellx][celly]!=EMPTY) {
      domove(cellx,celly,EMPTY,1); up=1;
    }
  } else if(event_mousebutton==SDL_BUTTON_RIGHT) {
    if(!v && m[cellx][celly]!=UNFILLED) {
      domove(cellx,celly,UNFILLED,1); up=1;
    } else if(v==1 && m[cellx][celly]!=EMPTY) {
      domove(cellx,celly,EMPTY,1); up=1;
    } else if(v==2 && m[cellx][celly]!=BLOCKED) {
      domove(cellx,celly,BLOCKED,1); up=1;
    }
  } else if(event_mousebutton==SDL_BUTTON_MIDDLE) {
    if(v && m[cellx][celly]!=UNFILLED) {
      domove(cellx,celly,UNFILLED,1); up=1;
    }
  }
  if(up) updatetoscreen(1),normalmove=1,numclicks++;
}

#define COLSIZE 6
static Uint32 colarray[]={
  0xFFDBFF, 0xFFFF00, 0xFFFFAA, 0xB6FF00, 0x92FFAA, 0xFFB600};

static int forcefullredraw;

static void drawverify() {
  int i,j,col=0,k,qs2;
  if(forcefullredraw) drawgrid();
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(!visit[i][j] && m[i][j]!=BLOCKED && m[i][j]!=UNFILLED) {
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

void kuromasu(char *path,int solve) {
	int event;
	loadpuzzle(path);
	drawgrid();
  if(solve) { autosolver(path); return; }
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
