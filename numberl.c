#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "SDL/SDL.h"
#include "puzzle.h"

/* max map size */
#define MAXS 128
/* max string length */
#define MAXSTR 1024
/* total rendered grid size is (x+mx) * (y+my) */
static int x,y;                 /*  map size */
static char difficulty[MAXSTR]; /*  string holding the difficulty */

/* 0: no number
   1-: number */
static int mn[MAXS][MAXS];

/* indicate whether there is an edge going to the cell to its right, or below */
/* 0: no edge, 1: edge */
static int m[2][MAXS][MAXS];

static char touched[MAXS][MAXS];  /*  1 if cell is changed and need to be redrawn */

/* move queue for hint system */
#define MAXMQ MAXS*MAXS*4
static int mq[MAXMQ];
static int mqs,mqe;

/* from a cell, edge directions */
static int ex[4]={0,0,-1,0},ey[4]={0,0,0,-1},ed[4]={0,1,0,1};

static int convchar(char *s,int *i) {
	char c=s[(*i)++];
	int v;
	if(!c) error("string in level definition ended prematurely.");
	else if(c=='.') return 0;
	else if(c>='1' && c<='9') return c-48;
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
			for(i=j=0;j<x;j++) mn[j][ln]=convchar(s,&i);
			ln++;
		}
	} else if(!gameinfo[0]) strcpy(gameinfo,s+2);
	fclose(f);
	/* set top left of grid area in window */
	startx=10,starty=(int)(font->height*2.5);
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
	if(thick) {
		/* left edge */
		drawrectangle32(startx+u*width,starty+v*height,startx+u*width+thick-1,starty+(v+1)*height-1,BLACK32);
		/* upper edge */
		drawrectangle32(startx+u*width,starty+v*height,startx+(u+1)*width-1,starty+v*height+thick-1,BLACK32);
	}
	/* empty cell */
	if(mn[u][v]==0) drawsolidcell32(u,v,blankcol);
	else if(mn[u][v]>0) drawnumbercell32(u,v,mn[u][v],BLACK32,BLACK32,blankcol);
	/* draw the edges */
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

/* do move bookkeeping, including putting it on the stack */
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

/* return the number of edges adjacent to a cell */
static int numedges(int u,int v) {
	int count=0;
	if(u && m[0][u-1][v]) count++;
	if(v && m[1][u][v-1]) count++;
	if(m[0][u][v]) count++;
	if(m[1][u][v]) count++;
	return count;
}

/* follow line at one end and return the coordinates of the other end
   or return -1,-1 if the start coordinate has degree!=1 */
static void followline(int x1,int y1,int *u2,int *v2) {
	int d,x0=-1,y0=-1,x2,y2;
	*u2=*v2=-1;
	if(x1<0 || y1<0 || x1>=x || y1>=y) return;
	if(numedges(x1,y1)!=1) return;
	do {
		for(d=0;d<4;d++) {
			x2=x1+dx[d]; y2=y1+dy[d];
			if(x2>=0 && y2>=0 && x2<x && y2<y && (x2!=x0 || y2!=y0) && m[ed[d]][x1+ex[d]][y1+ey[d]]==1) goto next;
		}
		error("sanity error in followline in numberlink");
	next:
		x0=x1; y0=y1;
		x1=x2; y1=y2;
	} while(numedges(x2,y2)==2);
	*u2=x2; *v2=y2;
}

// 1: 2x2 cell with upper left at (u,v) has u-turn
static int hasuturn(int u,int v) {
	int num=0;
	if(u<0 || v<0 || u>=x-1 || v>=y-1) return 0;
	if(m[0][u][v]) num++;
	if(m[1][u][v]) num++;
	if(m[0][u][v+1]) num++;
	if(m[1][u+1][v]) num++;
	return num>=3;
}

// 1: cell is saturated (has enough edges)
static int issaturated(int i,int j) {
	// everything outside the grid is saturated
	if(i<0 || j<0 || i>=x || j>=y) return 1;
	return numedges(i,j)==2-(mn[i][j]>0);
}

/* start of hint system! */

static void addmovetoqueue(int celld,int cellx,int celly,int val) {
	mq[mqe++]=celld; mq[mqe++]=cellx; mq[mqe++]=celly; mq[mqe++]=val;
	if(mqe==MAXMQ) mqe=0;
}

static int movequeueisempty() {
	return mqs==mqe;
}

/* return 0:no moves in queue, 1:move successfully executed */
static int executeonemovefromqueue(int visible) {
loop:
	if(movequeueisempty()) return 0;
	/* the hint system can produce some moves twice, don't redo moves */
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

/*static void silentexecutemovequeue() {
	while(executeonemovefromqueue(0));
}*/

/* return 1 if solved, 0 if unsolved, -1 if error somewhere */
static int verifyboard() {
	int i,j,num,ok=1,u,v;
	for(i=0;i<x;i++) for(j=0;j<y;j++) {
		/* check for illegal state: cell with >2 edges */
		num=numedges(i,j);
		if(num>2) return -1;
		// check for illegal state: cell with number and >1 edges
		if(num>1 && mn[i][j]>0) return -1;
		/* fewer than 2 edges in a non-number cell: not solved */
		if(mn[i][j]==0 && num<2) ok=0;
		/* fewer than 1 edge in a non-number cell: not solved */
		if(mn[i][j]>0 && num<1) ok=0;
	}
	/* follow lines and return error if the wrong numbers are connected */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]>0) {
		/* here, numedges==1 */
		followline(i,j,&u,&v);
		if(mn[u][v]>0 && mn[u][v]!=mn[i][j]) return -1;
	}
	return ok;
}

/* deeper check for unsolvability:
   - check if all numbers can be connected
     TODO can we use graph planarity stuff? max-flow?
*/
/*static int deepverify() {
	return 0;
}*/

//static void showverify() {}

// for a given number, if both of them are on the border and there's only one
// unblocked way to connect them (ie. the other direction has other numbers
// on it) connect the numbers along the edge.
// saturated cells along the border extends said border
static int level2numbersonedge() {
	return 0;
}

// if we can connect two numbers with one edge, do it
static int level2onemovefromconnect() {
	for(int i=0;i<x;i++) for(int j=0;j<y;j++) if(numedges(i,j)<2-(mn[i][j]>0)) {
		for(int d=0;d<4;d++) {
			int x2=i+dx[d],y2=j+dy[d];
			if(x2<0 || y2<0 || x2>=x || y2>=y) continue;       /* out of bounds? */
			if(m[ed[d]][i+ex[d]][j+ey[d]]) continue;           /* edge exists? */
			if(mn[x2][y2]>0 && mn[i][j]>0 && mn[x2][y2]!=mn[i][j]) continue; /* different number */
			if(mn[x2][y2]==0 && numedges(x2,y2)==2) continue;  /* degree 3 */
			if(mn[x2][y2]>0 && numedges(x2,y2)==1) continue;   /* degree 2 */
			int num1=mn[i][j];
			if(!num1) {
				int u,v;
				followline(i,j,&u,&v);
				if(u<0 || v<0) goto fail;
				num1=mn[u][v];
				if(!num1) goto fail;
			}
			int num2=mn[x2][y2];
			if(!num2) {
				int u,v;
				followline(x2,y2,&u,&v);
				if(u<0 || v<0) goto fail;
				num2=mn[u][v];
				if(!num2) goto fail;
			}
			if(num1==num2) {
				addmovetoqueue(ed[d],i+ex[d],j+ey[d],1);
				return 1;
			}
		fail:;
		}
	}
	return 0;
}

// if a cell has a missing edge, and only one placement leads to a legal
// position, do it
static int level2onegoodedge() {
	int found=0;
	for(int i=0;i<x;i++) for(int j=0;j<y;j++) if(mn[i][j]==0 && numedges(i,j)==1) {
		int u,v;
		followline(i,j,&u,&v);
		int othernum=mn[u][v];
		if(othernum==0) continue; // no number in other end: can't join different numbers
		int legal=0,dd=0;
		for(int d=0;d<4;d++) {
			int x2=i+dx[d],y2=j+dy[d];
			if(x2<0 || y2<0 || x2>=x || y2>=y) continue;       // out of bounds?
			if(m[ed[d]][i+ex[d]][j+ey[d]]) continue;           // edge exists
			if(numedges(x2,y2)==2-(mn[x2][y2]>0)) continue;    // destination cell already saturated
			if(mn[x2][y2]>0 && mn[x2][y2]!=othernum) continue; // connect 2 different numbers
			if(mn[x2][y2]==0 && numedges(x2,y2)==1) {
				// follow other line
				int u2,v2;
				followline(x2,y2,&u2,&v2);
				int othernum2=mn[u2][v2];
				if(othernum2>0 && othernum!=othernum2) continue; // connect 2 different numbers
			}
			legal++; dd=d;
		}
		if(legal==1) {
			// only one legal way to place edge, do it
			found=1;
			addmovetoqueue(ed[dd],i+ex[dd],j+ey[dd],1);
		}
	}
	return found;
}

static int level2hint() {
	if(level2onemovefromconnect()) return 1;
	if(level2onegoodedge()) return 1;
	if(level2numbersonedge()) return 1;
	return 0;
}

// because of uniqueness a U-turn contained within 2x2 cells can never occur.
// if only one move can avoid u-turn, do it
static int level1avoidushape() {
	for(int i=0;i<x;i++) for(int j=0;j<y;j++) if(numedges(i,j)<2-(mn[i][j]>0)) {
		// for each cell, try all moves, and reject those that would lead
		// to 2x2 U-turn or other immediately illegal positions.
		// if there's only one legal move left, do it
		int count=0,dd=0;
		for(int d=0;d<4;d++) {
			int x2=i+dx[d],y2=j+dy[d];
			if(x2<0 || y2<0 || x2>=x || y2>=y) continue;       /* out of bounds? */
			if(m[ed[d]][i+ex[d]][j+ey[d]]) continue;           /* edge exists? */
			if(mn[x2][y2]>0 && mn[i][j]>0 && mn[x2][y2]!=mn[i][j]) continue; /* different number */
			if(mn[x2][y2]==0 && numedges(x2,y2)==2) continue;  /* degree 3 */
			if(mn[x2][y2]>0 && numedges(x2,y2)==1) continue;   /* degree 2 */
			// check for 2x2 U-turn
			// temporarily create edge
			m[ed[d]][i+ex[d]][j+ey[d]]=1;
			if(hasuturn(i,j) || hasuturn(i-1,j) || hasuturn(i,j-1) || hasuturn(i-1,j-1)) {
				m[ed[d]][i+ex[d]][j+ey[d]]=0;
				continue;
			}
			m[ed[d]][i+ex[d]][j+ey[d]]=0;
			dd=d;
			if(++count>1) break;
		}
		if(count==1) {
			addmovetoqueue(ed[dd],i+ex[dd],j+ey[dd],1);
			return 1;
		}
	}
	return 0;
}

/* if there's only one trivially legal way to make an edge from a number, do it */
/* trivially illegal ways:
   - edge into an adjacent different number
   - edge into another line, making node degree 3
*/
static int level1forcededgenum() {
	int i,j,d,count,x2,y2,dd;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(mn[i][j]>0 && numedges(i,j)<1) {
		for(count=d=0;d<4;d++) {
			x2=i+dx[d]; y2=j+dy[d];
			if(x2<0 || y2<0 || x2>=x || y2>=y) continue;       /* out of bounds? */
			if(m[ed[d]][i+ex[d]][j+ey[d]]) continue;           /* edge exists? */
			if(mn[x2][y2]>0 && mn[x2][y2]!=mn[i][j]) continue; /* different number */
			if(mn[x2][y2]==0 && numedges(x2,y2)==2) continue;  /* degree 3 */
			dd=d;
			if(++count>1) break;
		}
		if(count==1) {
			addmovetoqueue(ed[dd],i+ex[dd],j+ey[dd],1);
			return 1;
		}
	}
	return 0;
}

// if we can make a corner near saturated cells, do it
// 
// ?S   if S are saturated cells and x is free and doesn't have a number, 
// Sx   make a corner at x
static int level1forcedcorner2() {
	int found=0;
	for(int i=0;i<x;i++) for(int j=0;j<y;j++) if(mn[i][j]==0 && numedges(i,j)==0) {
		// upper left
		if(issaturated(i-1,j) && issaturated(i,j-1) && i<x-1 && j<y-1) {
			found=1;
			if(!m[0][i][j]) addmovetoqueue(0,i,j,1);
			if(!m[1][i][j]) addmovetoqueue(1,i,j,1);
		}
		// upper right
		if(issaturated(i+1,j) && issaturated(i,j-1) && i>0 && j<y-1) {
			found=1;
			if(!m[0][i-1][j]) addmovetoqueue(0,i-1,j,1);
			if(!m[1][i][j]) addmovetoqueue(1,i,j,1);
		}
		// lower left
		if(issaturated(i-1,j) && issaturated(i,j+1) && i<x-1 && j>0) {
			found=1;
			if(!m[0][i][j]) addmovetoqueue(0,i,j,1);
			if(!m[1][i][j-1]) addmovetoqueue(1,i,j-1,1);
		}
		// lower right
		if(issaturated(i+1,j) && issaturated(i,j+1) && i>0 && j>0) {
			found=1;
			if(!m[0][i-1][j]) addmovetoqueue(0,i-1,j,1);
			if(!m[1][i][j-1]) addmovetoqueue(1,i,j-1,1);
		}
	}
	return found;
}

/* if there's only one trivially legal way to make an edge from a cell, do it */
/* trivially legal ways:
   - corner without number
*/
static int level1forcedcorner() {
	int found=0;
	for(int i=0;i<x;i+=x-1) for(int j=0;j<y;j+=y-1) if(mn[i][j]==0 && numedges(i,j)<2) {
		for(int d=0;d<4;d++) {
			int x2=i+dx[d],y2=j+dy[d];
			if(x2<0 || y2<0 || x2>=x || y2>=y) continue;       /* out of bounds? */
			if(m[ed[d]][i+ex[d]][j+ey[d]]) continue;           /* edge exists? */
			found=1;
			addmovetoqueue(ed[d],i+ex[d],j+ey[d],1);
		}
	}
	return found;
}

static int level1hint() {
	if(level1forcededgenum()) return 1;
	if(level1forcedcorner()) return 1;
	if(level1forcedcorner2()) return 1;
	if(level1avoidushape()) return 1;
	return 0;
}

static int hint() {
	usedhint=1;
	if(verifyboard()<0) return -1;
	if(level1hint()) return 1;
	if(level2hint()) return 1;
//	if(level3hint()) return 1;
//	if(level4hint()) return 1;
//	if(level5hint()) return 1;
	return 0;
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
		logprintf("dump m0 (horizontal)\n");
		for(j=0;j<y;j++) {
			for(i=0;i<x;i++) logprintf("%d",m[0][i][j]);
			logprintf("\n");
		}
		logprintf("dump m1 (vertical)\n");
		for(j=0;j<y;j++) {
			for(i=0;i<x;i++) logprintf("%d",m[1][i][j]);
			logprintf("\n");
		}
		logprintf("--------------\n");
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
			numclicks++; normalmove=1;
			updatetoscreen(celld,prevcellx,prevcelly,1);
		}
	}
}

void numberlink(char *path,int solve) {
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
		case EVENT_MOUSEMOTION:
			if(mousebuttons[0]) {
				processmousemotion();
				if(verifyboard()>0) {
					finalizetime(); displayscore(x,y);
					messagebox(1,"You are winner!");
					return;
				}
			}
			break;
		default:
			/*  catch intervals of values here */
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
