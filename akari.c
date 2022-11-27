#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "SDL/SDL.h"
#include "puzzle.h"

/* max map size */
#define MAXS 128
/* max string length */
#define MAXSTR 1024
static int x,y;                 /* map size */
static char difficulty[MAXSTR]; /* string holding the difficulty */

/* NB, we must have UNFILLED<LIGHT<EMPTY<WALLNO<WALL0 */
#define UNFILLED    -100
#define LIGHT       -3
#define EMPTY       -2
#define WALLNO      -1
#define WALL0       0
/* internal format:
   -100: unfilled cell
     -3:   cell with bulb
     -2:   empty cell (cannot contain bulb)
     -1:   wall without number
    0-4:  walls with number
*/
static int m[MAXS][MAXS];
#define LIGHTS_TOOFEW   0
#define LIGHTS_TOCLOSE  1
#define LIGHTS_OK       2
#define LIGHTS_ERROR    3
/* additional info:
   - for non-wall cells: number of bulbs shining on this cell
   - for wall cells, distinguish between:
     - the wall has too many bulbs next to it, or the number of bulbs plus
       the number of unfilled cells are lower than the requirement
     - the wall has a correct number of bulbs adjacent to it, but some of
       the adjacent cells are unfilled (TOCLOSE)
     - the wall has a correct number of bulbs next to it, and there are no
       adjacent unfilled cells (OK)
*/
/* different combinations:
   m=UNFILLED st=0     cell is unfilled, not lit up. this is our true "unfilled" cell
   m=EMPTY    st=0     cell is marked to be empty (no bulb goes here), not lt up
   m=LIGHT    st=1     cell has light bulb, and is therefore lit up
   m=UNFILLED st>=1    cell is unfilled, but is lit up from elsewhere
   m=EMPTY    st>=1    cell is marked to be empty (no bulb goes here) and is lit up
*/
static int st[MAXS][MAXS];
static char touched[MAXS][MAXS];  /* 1 if cell is changed and need to be redrawn */

/* move queue for hint system */
#define MAXMQ MAXS*MAXS*3
static int mq[MAXMQ];
static int mqs,mqe;

static int convchar(char *s,int *i) {
	char c=s[(*i)++];
	if(!c) error("string in level definition ended prematurely.");
	else if(c=='.') return UNFILLED;
	else if(c>='0' && c<='4') return WALL0+c-'0';
	else if(c=='#') return WALLNO;
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
			if(2!=sscanf(s,"%d %d",&x,&y)) error("expected x and y size");
			z++;
			break;
		case 3:
			for(i=j=0;j<x;j++) m[j][ln]=convchar(s,&i);
			ln++;
		}
	} else if(!gameinfo[0]) strcpy(gameinfo,s+2);
	if(fclose(f)) error("error reading file");
	startx=10,starty=(int)(font->height*2.5);
	mqs=mqe=0;
	for(i=0;i<x;i++) for(j=0;j<y;j++) {
		st[i][j]=touched[i][j]=0;
	}
}

/* helper routine: scan for combination of m[][] and st[][] */
static int scanforthing(int cellx,int celly,int dx,int dy,int findm,int findst) {
	while(cellx>=0 && cellx<x && celly>=0 && celly<y && m[cellx][celly]<WALLNO) {
		if(m[cellx][celly]==findm && st[cellx][celly]==findst) return 1;
		cellx+=dx; celly+=dy;
	}
	return 0;
}

/* helper routine: scan in one direction and return unfilled unlit cell if
   exactly one exists */
static int findoneunfilled(int cellx,int celly,int dx,int dy,int *cx,int *cy) {
	int count=0;
	while(cellx>=0 && cellx<x && celly>=0 && celly<y && m[cellx][celly]<WALLNO) {
		if(m[cellx][celly]==UNFILLED && !st[cellx][celly]) {
			*cx=cellx; *cy=celly;
			if(++count>1) break;
		}
		cellx+=dx; celly+=dy;
	}
	return count;
}

/* return 1 if solved, 0 if unsolved, -1 if the board is illegally filled in */
static int verifyboard() {
	int i,j,solved=1;
	for(i=0;i<x;i++) for(j=0;j<y;j++) {
		/* check for lights lighting up each other */
		if(m[i][j]==LIGHT && st[i][j]>1) return -1;
		/* check for wall having violated constraints */
		if(m[i][j]>=WALL0 && st[i][j]==LIGHTS_ERROR) return -1;
		if(m[i][j]<WALLNO && !st[i][j]) solved=0;
	}
	return solved;
}

/* deepverifyboard which checks for nontrivial contradictions like:
   - a lone empty unlit which cannot see any unfilled unlit
*/
static int deepverifyboard() {
	int r=verifyboard(),i,j,k,dummy;
	if(r) return r;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==EMPTY && !st[i][j]) {
		for(k=0;k<4;k++) if(findoneunfilled(i+dx[k],j+dy[k],dx[k],dy[k],&dummy,&dummy)) goto next;
		return -1;
	next:;
	}
	return 0;
}

static void updatecell(int u,int v) {
	int w=width<height?width:height;
	/* wall with no number */
	if(m[u][v]==WALLNO) drawsolidcell32(u,v,filledcol);
	/* unfilled */
	if(m[u][v]==UNFILLED) drawsolidcell32(u,v,st[u][v]?blankcol:unfilledcol);
	/* marked as empty */
	else if(m[u][v]==EMPTY) drawdisc(u,v,3,BLACK32,st[u][v]?blankcol:unfilledcol);
	/* light */
	else if(m[u][v]==LIGHT) drawdisc(u,v,(w-thick)*0.42,st[u][v]<2?lightcol:darkerrorcol,blankcol);
	else if(m[u][v]>=WALL0) {
		/* numbered wall */
		if(st[u][v]==LIGHTS_TOCLOSE) drawnumbercell32(u,v,m[u][v]-WALL0,almostokcol,almostokcol,filledcol);
		else if(st[u][v]==LIGHTS_OK) drawnumbercell32(u,v,m[u][v]-WALL0,okcol,okcol,filledcol);
		else if(st[u][v]==LIGHTS_ERROR) drawnumbercell32(u,v,m[u][v]-WALL0,errorcol,errorcol,filledcol);
		else drawnumbercell32(u,v,m[u][v]-WALL0,WHITE32,WHITE32,filledcol);
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

/* warning, only call this function on a cell with a numbered wall */
static void updatewall(int cellx,int celly) {
	int i,cx,cy,lights=0,free=0,lit=0,val=m[cellx][celly]-WALL0,verdict;
	if(m[cellx][celly]<WALL0) error("logical error, cell is not numbered wall");
	for(i=0;i<4;i++) {
		cx=cellx+dx[i]; cy=celly+dy[i];
		if(cx<0 || cy<0 || cx>=x || cy>=y || m[cx][cy]>=WALLNO) continue;
		if(m[cx][cy]==LIGHT) lights++;
		else if(m[cx][cy]==UNFILLED && !st[cx][cy]) free++;
		else lit++;
	}
	if(lights==val && !free) verdict=LIGHTS_OK;
	else if(lights>val || lights+free<val) verdict=LIGHTS_ERROR;
	else if(lights+free==val || lights==val) verdict=LIGHTS_TOCLOSE;
	else verdict=LIGHTS_TOOFEW;
	if(st[cellx][celly]!=verdict) {
		st[cellx][celly]=verdict;
		touched[cellx][celly]=1;
	}
}

static void checknearbywalls(int cellx,int celly) {
	int cx,cy,i;
	for(i=0;i<4;i++) {
		cx=cellx+dx[i]; cy=celly+dy[i];
		if(cx<0 || cy<0 || cx>=x || cy>=y || m[cx][cy]<WALL0) continue;
		updatewall(cx,cy);
	}
}

/* shine on one cell and update light colour */
static void shineone(int cellx,int celly,int inc) {
	st[cellx][celly]+=inc;
	if(st[cellx][celly]==(inc>0?1:0)) {
		checknearbywalls(cellx,celly);
		touched[cellx][celly]=1;
	}
	if(m[cellx][celly]==LIGHT && st[cellx][celly]==(inc>0?2:1)) touched[cellx][celly]=1;
}

/* spread the light in a given direction */
static void shine(int cellx,int celly,int dx,int dy,int inc) {
	while(cellx>=0 && celly>=0 && cellx<x && celly<y && m[cellx][celly]<WALLNO) {
		shineone(cellx,celly,inc);
		cellx+=dx; celly+=dy;
	}
}

static void shineall(int cellx,int celly,int inc) {
	int i;
	shineone(cellx,celly,inc);
	for(i=0;i<4;i++) shine(cellx+dx[i],celly+dy[i],dx[i],dy[i],inc);
}

/* do move bookkeeping, including putting it on the stack */
static void domove(int cellx,int celly,int val) {
	if(val==m[cellx][celly]) error("logical error, tried to set cell to existing value");
	stackpush(cellx); stackpush(celly); stackpush(m[cellx][celly]);
	m[cellx][celly]=val;
	touched[cellx][celly]=1;
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

static void updatetoscreen(int cellx,int celly,int oldval,int visible) {
	/* light up from cell */
	if(oldval==LIGHT) shineall(cellx,celly,-1);
	else if(m[cellx][celly]==LIGHT) shineall(cellx,celly,1);
	/* affect nearby numbered walls */
	checknearbywalls(cellx,celly);
	if(visible) partialredraw();
}

static void undo(int visible) {
	if(!stackempty()) {
		int val=stackpop(),celly=stackpop(),cellx=stackpop();
		int old=m[cellx][celly];
		m[cellx][celly]=val;
		touched[cellx][celly]=1;
		updatetoscreen(cellx,celly,old,visible);
	}
}

static int togglecell(int val) {
	switch(val) {
	case UNFILLED:
	case EMPTY:
		return LIGHT;
	case LIGHT:
		return EMPTY;
	default:
		error("wrong value.");
	}
	return 0;
}

/* change board according to mouse click */
static void processmousedown() {
	int cellx,celly,v=controlscheme_akari,up=0,old;
	getcell(event_mousex,event_mousey,&cellx,&celly);
	if(cellx<0 || celly<0 || cellx>=x || celly>=y || m[cellx][celly]>=WALLNO) return;
	old=m[cellx][celly];
	if(event_mousebutton==SDL_BUTTON_LEFT) {
		if(!v) {
			domove(cellx,celly,togglecell(m[cellx][celly])); up=1; normalmove=1; numclicks++;
		} else if(v==1 && m[cellx][celly]!=LIGHT) {
			domove(cellx,celly,LIGHT); up=1; normalmove=1; numclicks++;
		} else if(v==2 && m[cellx][celly]!=EMPTY) {
			domove(cellx,celly,EMPTY); up=1; normalmove=1; numclicks++;
		}
	} else if(event_mousebutton==SDL_BUTTON_RIGHT) {
		if(!v && m[cellx][celly]!=UNFILLED) {
			domove(cellx,celly,UNFILLED); up=1; normalmove=1; numclicks++;
		} else if(v==1 && m[cellx][celly]!=EMPTY) {
			domove(cellx,celly,EMPTY); up=1; normalmove=1; numclicks++;
		} else if(v==2 && m[cellx][celly]!=LIGHT) {
			domove(cellx,celly,LIGHT); up=1; normalmove=1; numclicks++;
		}
	} else if(event_mousebutton==SDL_BUTTON_MIDDLE) {
		if(v && m[cellx][celly]!=UNFILLED) {
			domove(cellx,celly,UNFILLED); up=1; normalmove=1; numclicks++;
		}
	}
	if(up) updatetoscreen(cellx,celly,old,1);
}

/* start of hint system! */

static void addmovetoqueue(int cellx,int celly,int val) {
	mq[mqe++]=cellx; mq[mqe++]=celly; mq[mqe++]=val;
	if(mqe==MAXMQ) mqe=0;
}

static int movequeueisempty() {
	return mqs==mqe;
}

/* return 0:no moves in queue, 1:move successfully executed */
static int executeonemovefromqueue(int visible) {
	int old;
loop:
	if(movequeueisempty()) return 0;
	/* the hint system can produce some moves twice, don't redo moves */
	if(m[mq[mqs]][mq[mqs+1]]==mq[mqs+2]) {
		mqs+=3;
		if(mqs==MAXMQ) mqs=0;
		goto loop;
	}
	old=m[mq[mqs]][mq[mqs+1]];
	domove(mq[mqs],mq[mqs+1],mq[mqs+2]);
	updatetoscreen(mq[mqs],mq[mqs+1],old,visible);
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

/* check if there are any numbered walls which can be trivially filled in:
   let the wall have number n. fill in if lit==vall or lit+free==vall. */
static int level1trivialwall() {
	int i,j,k,lights=0,free=0,lit=0,cx,cy,val;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>=WALL0 && st[i][j]==LIGHTS_TOCLOSE) {
		val=m[i][j]-WALL0;
		for(k=0;k<4;k++) {
			cx=i+dx[k]; cy=j+dy[k];
			if(cx<0 || cy<0 || cx>=x || cy>=y || m[cx][cy]>=WALLNO) continue;
			if(m[cx][cy]==LIGHT) lights++;
			else if(st[cx][cy] || m[cx][cy]==EMPTY) lit++;
			else free++;
		}
		if(lights+free==val) {
			for(k=0;k<4;k++) {
				cx=i+dx[k]; cy=j+dy[k];
				if(cx<0 || cy<0 || cx>=x || cy>=y || m[cx][cy]!=UNFILLED || st[cx][cy]) continue;
				addmovetoqueue(cx,cy,LIGHT);
			}
			return 1;
		}
		if(lights==val) {
			for(k=0;k<4;k++) {
				cx=i+dx[k]; cy=j+dy[k];
				if(cx<0 || cy<0 || cx>=x || cy>=y || m[cx][cy]!=UNFILLED || st[cx][cy]) continue;
				addmovetoqueue(cx,cy,EMPTY);
			}
			return 1;
		}
	}
	return 0;
}

static int level1hint() {
	if(level1trivialwall()) return 1;
	return 0;
}

/* if a cell will prevent a diagonally adjacent wall to fulfill its
   light requirement, that cell must be empty */
static int level2diagonallight() {
	int i,j,k,l,cx,cy,x2,y2,lights,free,val;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED && !st[i][j]) {
		/* check for diagonally adjacent numbered wall */
		for(k=4;k<8;k++) {
			cx=i+dx8[k]; cy=j+dy8[k];
			if(cx>=0 && cy>=0 && cx<x && cy<y && m[cx][cy]>=WALL0) {
				lights=free=0;
				val=m[cx][cy]-WALL0;
				for(l=0;l<4;l++) {
					x2=cx+dx[l]; y2=cy+dy[l];
					if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]>=WALLNO) continue;
					if(manhattandist(i,j,x2,y2)==1) continue;
					if(m[x2][y2]==LIGHT) lights++;
					else if(m[x2][y2]==UNFILLED && !st[x2][y2]) free++;
				}
				if(val>lights+free) {
					addmovetoqueue(i,j,EMPTY);
					return 1;
				}
			}
		}
	}
	return 0;
}

/* find an empty unlit cell which is the only way to light up one other cell */
static int level2onepossibility() {
	int i,j,k,cx=0,cy=0,count,res;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==EMPTY && !st[i][j]) {
		count=0;
		for(k=0;k<4;k++) {
			if(!(res=findoneunfilled(i+dx[k],j+dy[k],dx[k],dy[k],&cx,&cy))) continue;
			if(res>1 || ++count>1) goto next;
		}
		if(!count) continue;
		addmovetoqueue(cx,cy,LIGHT);
		return 1;
	next:;
	}
	return 0;
}

/* if an unfilled cell can't see any other cell, it must contain bulb */
static int level2alone() {
	int i,j,k,ok=0;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED && !st[i][j]) {
		/* trace in all directions */
		for(k=0;k<4;k++) if(scanforthing(i+dx[k],j+dy[k],dx[k],dy[k],UNFILLED,0)) goto next;
		addmovetoqueue(i,j,LIGHT);
		ok=1;
	next:;
	}
	return ok;
}

static int level2hint() {
	if(level2diagonallight()) return 1;
	if(level2onepossibility()) return 1;
	if(level2alone()) return 1;
	return 0;
}

static int level3hint() {
	return 0;
}

static int level4hint() {
	return 0;
}

static int lev5alt1[MAXS][MAXS];
static int lev5alt2[MAXS][MAXS];

/* copy board from a to b */
static void copyboard(int a[MAXS][MAXS],int b[MAXS][MAXS]) {
	int i,j;
	for(i=0;i<x;i++) for(j=0;j<y;j++) b[i][j]=a[i][j];
}

static int dogreedy(int lev) {
	int r;
	while(1) {
		r=deepverifyboard();
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
	int z=x*y,old,r,k,l,oldsp=getstackpos();
	if(i>=x) i=0;
	if(j>=y) j=0;
	while(z--) {
		if(m[i][j]==UNFILLED && !st[i][j]) {
			/* assume bulb */
			old=m[i][j];
			domove(i,j,LIGHT);
			updatetoscreen(i,j,old,0);
			r=dogreedy(lev);
			if(r<0) {
				/* contradiction! */
				while(getstackpos()>oldsp) undo(0);
				addmovetoqueue(i,j,EMPTY);
				return 1;
			}
			copyboard(m,lev5alt1);
			while(getstackpos()>oldsp) undo(0);
			/* assume empty */
			domove(i,j,EMPTY);
			updatetoscreen(i,j,old,0);
			r=dogreedy(lev);
			if(r<0) {
				/* contradiction! */
				while(getstackpos()>oldsp) undo(0);
				addmovetoqueue(i,j,LIGHT);
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
	/* next line is only needed when we actually have level 4 hints */
/*	if(level5contradiction(4)) return 1;*/
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
	} else if(key==SDLK_j) {  /* temporary: superhintkey */
		res=hint();
		if(res>0) {
			executemovequeue();
			while(hint()>0) executemovequeue();
			if(verifyboard()<1) messagebox(1,"Sorry, no more moves found.");
		} else if(!res) messagebox(1,"Sorry, no moves found.");
		else messagebox(1,"Sorry, hint will not work on an illegal board.");
	}
	else if(key==SDLK_d) {
		int i,j;
		logprintf("dump m\n");
		for(j=0;j<y;j++) {
			for(i=0;i<x;i++) if(m[i][j]==UNFILLED) logprintf("?");
			else if(m[i][j]==EMPTY) logprintf(".");
			else if(m[i][j]==LIGHT) logprintf("*");
			else if(m[i][j]==WALLNO) logprintf("#");
			else logprintf("%d",m[i][j]-WALL0);
			logprintf("\n");
		}
		logprintf("dump st\n");
		for(j=0;j<y;j++) {
			for(i=0;i<x;i++) {
				logprintf("%d ",st[i][j]);
			}
			logprintf("\n");
		}
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

void akari(char *path,int solve) {
	int event,i,j;
	loadpuzzle(path);
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>=WALL0) updatewall(i,j);
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
			/* catch intervals of values here */
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
