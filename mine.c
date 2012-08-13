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

/*  NB, we must have UNFILLED,EMPTY,MINE<UNAVAIL<0 */
#define NOINTERSECT -99
#define NOVALUE -101

#define UNFILLED    -100
#define EMPTY       -3
#define MINE				-2
#define UNAVAIL			-1
/*  internal format:
    -100: unfilled cell
    -3:   empty cell
    -2:   mine
		-1:   unavailable (blocked by design)
    0-8:  numbered cell
*/
static int m[MAXS][MAXS];
/* no st[][], it's calculated on the fly */
static char touched[MAXS][MAXS]; /* 1 if cell needs to be redrawn */
static char touchcount;          /* 1 if mine counter needs to be redrawn */
static int maxmines;

static int curmines; /* current number of mines in game */

/*  move queue for hint system */
#define MAXMQ MAXS*MAXS*3
static int mq[MAXMQ];
static int mqs,mqe;

static int convchar(char *s,int *i) {
  char c=s[(*i)++];
  if(!c) error("string in level definition ended prematurely.");
  else if(c=='.') return UNFILLED;
	else if(c=='x') return UNAVAIL;
	else if(c>='0' && c<='8') return c-48;
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
      for(i=j=0;j<x;j++) m[j][ln]=convchar(s,&i);
      ln++;
			if(ln==y) goto done;
    }
  }
done:
	while(fgets(s,MAXSTR,f)) if(s[0]!='%') {
		if(sscanf(s,"mines %d",&maxmines)!=1) error("didn't find mines entry\n");
		break;
	}
  fclose(f);
  startx=10,starty=30;
  mqs=mqe=0;
  for(i=0;i<x;i++) for(j=0;j<y;j++) touched[i][j]=0;
	touchcount=curmines=0;
}

static void count(int u,int v,int *unfilled,int *empty,int *mine) {
	int i,x2,y2;
	*unfilled=*empty=*mine=0;
	for(i=0;i<8;i++) {
		x2=u+dx8[i]; y2=v+dy8[i];
		if(x2>=0 && x2<x && y2>=0 && y2<y) {
			if(m[x2][y2]==UNFILLED) (*unfilled)++;
			else if(m[x2][y2]==EMPTY) (*empty)++;
			else if(m[x2][y2]==MINE) (*mine)++;
		}
	}
}

static void updatecell(int u,int v) {
	int unfilled,empty,mine,w;
	Uint32 col=0;
	if(m[u][v]==UNFILLED) drawsolidcell32(u,v,unfilledcol);
	else if(m[u][v]==EMPTY) drawsolidcell32(u,v,blankcol);
	else if(m[u][v]==MINE) {
		w=width<height?width:height;
		drawdisc(u,v,(w-thick)*0.28,filledcol,blankcol);
	} else if(m[u][v]==UNAVAIL) drawcross(u,v,unfilledcol,filled2col);
	else if(m[u][v]>-1) {
		count(u,v,&unfilled,&empty,&mine);
		if(m[u][v]==mine && !unfilled) col=okcol;
		else if(m[u][v]<mine) col=errorcol;
		else if(m[u][v]==mine+unfilled || m[u][v]==mine) col=almostokcol;
		else col=blankcol;
		drawnumbercell32(u,v,m[u][v],filledcol,filledcol,col);
	}
}

static void updateminesleft() {
	static int prev=0;
	int w;
	char s[128];
	if(!maxmines) sprintf(s,"Mines left: N/A");
	else sprintf(s,"Mines left: %d\n",maxmines-curmines);
	w=sdl_font_width(font,"%s",s);
	drawrectangle32(startx,starty+(y+1)*height,startx+prev-1,starty+(y+2)*height-1,WHITE32);
	sdl_font_printf(screen,font,startx,starty+(y+1)*height,BLACK32,GRAY32,"%s",s);
	prev=w;
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
  for(i=0;i<x;i++) for(j=0;j<y;j++) updatecell(i,j);
	updateminesleft();
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
}

static void partialredraw() {
  int i,j;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(touched[i][j]) updatecell(i,j);
	if(touchcount) updateminesleft();
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  for(i=0;i<x;i++) for(j=0;j<y;j++) if(touched[i][j]) {
    sdlrefreshcell(i,j);
    touched[i][j]=0;
  }
	if(touchcount) {
		touchcount=0;
		SDL_UpdateRect(screen,0,starty+(y+1)*height,resx,height);
	}
}

static void updatetoscreen(int visible) {
  if(visible) partialredraw();
}

static void applymove(int cellx,int celly,int val,int visible) {
	int i,x2,y2;
	if(m[cellx][celly]==MINE && val!=MINE) curmines--,touchcount=visible;
	else if(m[cellx][celly]!=MINE && val==MINE) curmines++,touchcount=visible;
	m[cellx][celly]=val;
	if(visible) {
		touched[cellx][celly]=1;
		for(i=0;i<8;i++) {
			x2=cellx+dx8[i]; y2=celly+dy8[i];
			if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]<0) continue;
			touched[x2][y2]=1;
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

/*  do move bookkeeping, including putting it on the stack */
static void domove(int cellx,int celly,int val,int visible) {
  if(val==m[cellx][celly]) error("logical error, tried to set cell to existing value");
  stackpush(cellx); stackpush(celly); stackpush(m[cellx][celly]);
  applymove(cellx,celly,val,visible);
}

/* trivial overestimate: return sum of unsatisfied numbers */
/* overestimate doesn't care about unfilled cells not adjacent to
   numbers! */
/* keep this code in case the better overestimate is incorrect */
static int trivialoverestimate() {
	int est=0,i,j,unfilled,empty,mine;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>-1) {
		count(i,j,&unfilled,&empty,&mine);
		est+=m[i][j]-mine;
	}
	return est;
}

/* trivial underestimate: return max of unsatisfied numbers */
/* keep this code in case the better underestimate is incorrect */
static int trivialunderestimate() {
	int maks=0,i,j,unfilled,empty,mine;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>-1) {
		count(i,j,&unfilled,&empty,&mine);
		if(maks<m[i][j]-mine) maks=m[i][j]-mine;
	}
	return maks;
}

/* overestimate: find the sum of all unsatisfied numbered cells,
   then find the number of numbered neighbours of each unfilled cell.
	 sort them in nondecreasing order, and pick cells greedily until
	 picking a new cell causes the sum of the picked cells to exceed the
	 sum of all unsatisfied numbered cells. */
static int overestimate() {
	static int a[MAXS*MAXS];
	int n=0,i,j,sum,unfilled,empty,mine,d,x2,y2,cur;
	/* find sum */
	for(sum=i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>-1) {
		count(i,j,&unfilled,&empty,&mine);
		sum+=m[i][j]-mine;
	}
	/* find all unfilled cells with only unsatisfied neighbours */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
		for(cur=d=0;d<8;d++) {
			x2=i+dx8[d]; y2=j+dy8[d];
			if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]<0) continue;
			count(x2,y2,&unfilled,&empty,&mine);
			if(mine==m[x2][y2]) goto next;
			cur++;
		}
		if(!cur) continue;
		a[n++]=cur;
	next:;
	}
	qsort(a,n,sizeof(int),compi);
	for(i=0;i<n;i++) {
		sum-=a[i];
		if(sum<1) return i+1;
	}
	return 0;
}

static int compirev(const void *A,const void *B) {
	const int *a=A,*b=B;
	if(*a>*b) return -1;
	if(*a<*b) return 1;
	return 0;
}

/* underestimate: find the sum of all unsatisfied numbered cells,
   then find the number of numbered neighbours of each unfilled cell.
	 sort them in nonincreasing order, and pick cells greedily until
	 the sum of picked cells exceed sum of unsatisfied numbered cells. */
static int underestimate() {
	static int a[MAXS*MAXS];
	int n=0,i,j,sum,unfilled,empty,mine,d,x2,y2,cur;
	/* find sum */
	for(sum=i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>-1) {
		count(i,j,&unfilled,&empty,&mine);
		sum+=m[i][j]-mine;
	}
	/* find all unfilled cells with only unsatisfied neighbours */
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
		for(cur=d=0;d<8;d++) {
			x2=i+dx8[d]; y2=j+dy8[d];
			if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]<0) continue;
			count(x2,y2,&unfilled,&empty,&mine);
			if(mine==m[x2][y2]) goto next;
			cur++;
		}
		a[n++]=cur;
	next:;
	}
	qsort(a,n,sizeof(int),compirev);
	for(i=0;i<n;i++) {
		sum-=a[i];
		if(!sum) return i;
		if(sum<0) return i+1;
	}
	return 0;
}

static int verifyboard() {
	int totmines=0,i,j,unfilled,empty,mine,incomplete=0,isolate,x2,y2,d;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==MINE) totmines++;
	else if(m[i][j]==UNFILLED) incomplete=1;
	else if(m[i][j]>-1) {
		count(i,j,&unfilled,&empty,&mine);
		if(mine+unfilled<m[i][j]) return -1;
		if(mine>m[i][j]) return -1;
	}
	if(maxmines && totmines>maxmines) return -1;
	if(totmines<maxmines) incomplete=1;
	/* apply estimates for puzzles with mines left defined!
	   find over- and underestimates for the number of mines that can fit
		 on the current board. we want the underestimate to be as high as
		 possible (without exceeding the correct amount) and the overestimate
		 to be as low as possible.
	   if mines placed + underestimate < maxmines then board is illegal
	   if mines placed + overestimate > maxmines then board is illegal */
	if(maxmines && incomplete) {
		/* need to calculate number of unfilled cells not touching any
		   numbered cells */
		isolate=0;
		for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED) {
			for(d=0;d<8;d++) {
				x2=i+dx8[d],y2=j+dy8[d];
				if(x2<0 || y2<0 || x2>=x || y2>=y) continue;
				if(m[x2][y2]>-1) goto next;
			}
			isolate++;
		next:;
		}
		if(totmines+isolate+overestimate()<maxmines) return -1;
		if(totmines+underestimate()>maxmines) return -1;
	}
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

/* if the rest of the cells around a number can only be filled in one way,
   do it (the rest must be bomb or the rest must be empty) */
static int level1forced() {
	int i,j,unfilled,empty,mine,x2,y2,d,fill,ok=0;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]>-1) {
		count(i,j,&unfilled,&empty,&mine);
		fill=UNFILLED;
		if(unfilled && mine+unfilled==m[i][j]) fill=MINE;
		else if(unfilled && mine==m[i][j]) fill=EMPTY;
		if(fill!=UNFILLED) {
			ok=1;
			for(d=0;d<8;d++) {
				x2=i+dx8[d]; y2=j+dy8[d];
				if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]!=UNFILLED) continue;
				addmovetoqueue(x2,y2,fill);
			}
		}
	}
	return ok;
}

static int level1restisemptyormine() {
	int i,j,mines=0,unfilled=0,fill=UNFILLED;
	if(!maxmines) return 0;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==MINE) mines++;
	else if(m[i][j]==UNFILLED) unfilled++;
	if(!unfilled) return 0;
	if(mines==maxmines) fill=EMPTY;
	else if(mines+unfilled==maxmines) fill=MINE;
	else return 0;
	for(i=0;i<x;i++) for(j=0;j<y;j++) if(m[i][j]==UNFILLED)
		addmovetoqueue(i,j,fill);
	return 1;
}

static int level1hint() {
	if(level1forced()) return 1;
	if(level1restisemptyormine()) return 1;
	return 0;
}

int snoob(int x) {
	int s=x&-x, r=x+s, o=x^r;
	o=(o>>2)/s;
	return r|o;
}

/* try all combinations of placing the mines around a number. reject those
   combinations that violate other numbers within a 5x5 area around the
   current number. then take intersection of all legal combinations */
static int level2tryallinterference() {
	int z=x*y,unfilled,empty,mine,need,d,minemask,emptymask,u2,e2,m2;
	int ex[8],ey[8],e,x2,y2,k,l,curm,cure,mask;
	static int i=0,j=0;
	if(i>=x) i=0; if(j>=y) j=0;
	while(z--) {
		if(m[i][j]<0) goto next;
		count(i,j,&unfilled,&empty,&mine);
		if(!unfilled) goto next;
		need=m[i][j]-mine;
		minemask=(1<<unfilled)-1;
		emptymask=(1<<unfilled)-1;
		mask=(1<<need)-1;
		for(d=e=0;d<8;d++) {
			x2=i+dx8[d]; y2=j+dy8[d];
			if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]!=UNFILLED) continue;
			ex[e]=x2; ey[e++]=y2;
		}
		while(mask<(1<<unfilled)) {
			curm=cure=0;
			for(d=0;d<e;d++) {
				x2=ex[d]; y2=ey[d];
				if(mask&(1<<d)) domove(x2,y2,MINE,0),curm|=1<<d;
				else domove(x2,y2,EMPTY,0),cure|=1<<d;
			}
			for(k=-2;k<3;k++) for(l=-2;l<3;l++) {
				if(i+k<0 || i+k>=x || j+l<0 || j+l>=y || m[i+k][j+l]<0) continue;
				count(i+k,j+l,&u2,&e2,&m2);
				if(m2+u2<m[i+k][j+l]) goto illegal;
				if(m2>m[i+k][j+l]) goto illegal;
			}
			for(d=0;d<e;d++) undo(0);
			minemask&=curm;
			emptymask&=cure;
			if(!(minemask|emptymask)) goto next;
			goto nextmask;
		illegal:
			for(d=0;d<e;d++) undo(0);
		nextmask:
			mask=snoob(mask);
		}
		if(minemask|emptymask) {
			for(d=0;d<e;d++) if(minemask&(1<<d)) addmovetoqueue(ex[d],ey[d],MINE);
			else if(emptymask&(1<<d)) addmovetoqueue(ex[d],ey[d],EMPTY);
			return 1;
		}
	next:
		j++;
		if(j==y) {
			j=0; i++;
			if(i==x) i=0;
		}
	}
	return 0;
}

static int level2hint() {
	if(level2tryallinterference()) return 1;
	return 0;
}

static int level3hint() {
	return 0;
}

static int level4hint() {
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

/* try all ways of placing mines around a number and take intersection */
static int level5tryallways() {
	static int i=0,j=0;
	int z=x*y,k,l,unfilled,empty,mine,e,d,ex[8],ey[8],x2,y2,mask,need;
	int oldsp=getstackpos(),ok=0;
	if(i>=x) i=0; if(j>=y) j=0;
	while(z--) {
		if(m[i][j]<0) goto next;
		count(i,j,&unfilled,&empty,&mine);
		if(!unfilled) goto next;
		for(k=0;k<x;k++) for(l=0;l<y;l++) lev5m[k][l]=NOVALUE;
		for(d=e=0;d<8;d++) {
			x2=i+dx8[d]; y2=j+dy8[d];
			if(x2<0 || y2<0 || x2>=x || y2>=y || m[x2][y2]!=UNFILLED) continue;
			ex[e]=x2; ey[e++]=y2;
		}
		/* try all */
		need=m[i][j]-mine;
		mask=(1<<need)-1;
		while(mask<(1<<unfilled)) {
			for(d=0;d<e;d++) {
				x2=ex[d]; y2=ey[d];
				if(mask&(1<<d)) domove(x2,y2,MINE,0);
				else domove(x2,y2,EMPTY,0);
			}
			if(dogreedy(4)>-1) for(k=0;k<x;k++) for(l=0;l<y;l++) {
				if(lev5m[k][l]==NOVALUE && m[k][l]!=UNFILLED) lev5m[k][l]=m[k][l];
				else if(lev5m[k][l]!=m[k][l]) lev5m[k][l]=NOINTERSECT;
			}
			while(getstackpos()>oldsp) undo(0);
			mask=snoob(mask);
		}
		for(k=0;k<x;k++) for(l=0;l<y;l++) {
			if(lev5m[k][l]!=NOVALUE && lev5m[k][l]!=NOINTERSECT && lev5m[k][l]!=m[k][l])
				addmovetoqueue(k,l,lev5m[k][l]),ok=1;
		}
		if(ok) return 1;
	next:
		j++;
		if(j==y) {
			j=0; i++;
			if(i==x) i=0;
		}
	}
	return 0;
}

static int level5hint() {
	if(level5tryallways()) return 1;
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

static int togglecell(int val) {
  switch(val) {
  case UNFILLED:
  case EMPTY:
    return MINE;
  case MINE:
    return EMPTY;
  default:
    error("wrong value.");
  }
  return 0;
}

/*  change board according to mouse click */
static void processmousedown() {
  int cellx,celly,v=controlscheme_mine,up=0;
  getcell(event_mousex,event_mousey,&cellx,&celly);
  if(cellx<0 || celly<0 || cellx>=x || celly>=y || m[cellx][celly]>-1 || m[cellx][celly]==UNAVAIL) return;
  if(event_mousebutton==SDL_BUTTON_LEFT) {
    if(!v) {
      domove(cellx,celly,togglecell(m[cellx][celly]),1); up=1;
    } else if(v==1 && m[cellx][celly]!=MINE) {
      domove(cellx,celly,MINE,1); up=1;
    } else if(v==2 && m[cellx][celly]!=EMPTY) {
      domove(cellx,celly,EMPTY,1); up=1;
    }
  } else if(event_mousebutton==SDL_BUTTON_RIGHT) {
    if(!v && m[cellx][celly]!=UNFILLED) {
      domove(cellx,celly,UNFILLED,1); up=1;
    } else if(v==1 && m[cellx][celly]!=EMPTY) {
      domove(cellx,celly,EMPTY,1); up=1;
    } else if(v==2 && m[cellx][celly]!=MINE) {
      domove(cellx,celly,MINE,1); up=1;
    }
  } else if(event_mousebutton==SDL_BUTTON_MIDDLE) {
    if(v && m[cellx][celly]!=UNFILLED) {
      domove(cellx,celly,UNFILLED,1); up=1;
    }
  }
  if(up) updatetoscreen(1);
}

static void processkeydown(int key) {
  int res;
  if(key==undokey) undo(1);
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

void mine(char *path,int solve) {
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
