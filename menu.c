#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "SDL/SDL.h"
#include "dir.h"
#include "puzzle.h"
#include "sdlfont.h"

#define MAXID 16
#define MAXDISP 32
#define MAXDIFF 32

#define MAXSTRING 65536

static int logox,logoy; /* logo size */
static int buttonwidth,buttonheight,buttony; /* button size, gap between buttons */
static int bottomy; /* free space under buttons/puzzle list */
static int tablex; /* x-coordinate of left side of table */
static int tablebottomy; /* y-coordinate of bottom of table */
static int numentries; /* number of entries to show in table */
static int bottomair; /* y-space between bottom of table and text */
static int rightair; /* x-space to the right of table */

/* fields in cache file:
   filename,proper name,difficulty,x,y,date (yyyy-mm-dd-hh-mm),hundredths,clicks
   three last fields are empty if unsolved */
typedef struct {
	char puzzleid[MAXID];
	char display[MAXDISP];
	char difficulty[MAXDIFF];
	int xsize,ysize;
	/* score stuff */
	int year,month,date,hour,minute; /* year=0 is unsolved */
	int hundredths;
	int clicks;
	/* tiebreaker for sort */
	int ix;
} entry_t;

#define STARTNUM 1000
static entry_t *puzzlelist=NULL;
static int puzzlenum,puzzlealloc;

static entry_t *newentry() {
	if(puzzlenum==puzzlealloc) {
		puzzlealloc<<=1;
		puzzlelist=realloc(puzzlelist,sizeof(entry_t)*puzzlealloc);
		if(!puzzlelist) error("out of memory\n");
	}
	return puzzlelist+puzzlenum++;
}

static int hascomma(char *s) {
	int comma=0;
	while(*s) comma+=*(s++)==',';
	return comma==11;
}

static int findcomma(char *s,int p) {
	while(s[p] && s[p]!=',') p++;
	return p;
}

/* warning, rewrite this routine to make it much faster by doing several
   passes: first pass which only loads filenames, sort file and remove
	 duplicates, then load puzzle files that has filename entries only */
static void loadcache() {
	static char filename[MAXSTRING];
	static char line[MAXSTRING];
	entry_t *e;
	int p,q;
	FILE *f;
	if(puzzlelist==NULL) {
		puzzlelist=malloc(sizeof(entry_t)*STARTNUM);
		if(!puzzlelist) error("out of memory loading cache\n");
		puzzlealloc=STARTNUM;
		puzzlenum=0;
	}
	if(strlen(puzzlepath)+strlen("cache.txt")>=MAXSTRING-1)
		error("filename too long\n");
	strcpy(filename,puzzlepath);
	strcat(filename,"cache.txt");
	f=fopen(filename,"r");
	if(f) {
		while(fgets(line,MAXSTRING,f)) if(hascomma(line)) {
			e=newentry();
			p=0; q=findcomma(line,p); line[q]=0;
			if(strlen(line)>=MAXID-1) error("loadcache: string too long\n");
			strcpy(e->puzzleid,line);
			p=q+1; q=findcomma(line,p); line[q]=0;
			if(strlen(line+p)>=MAXDISP-1) error("loadcache: string too long\n");
			strcpy(e->display,line+p);
			p=q+1; q=findcomma(line,p); line[q]=0;
			if(strlen(line+p)>=MAXDIFF-1) error("loadcache: string too long\n");
			strcpy(e->difficulty,line+p);
			p=q+1; q=findcomma(line,p); line[q]=0; e->xsize=strtol(line+p,0,10);
			p=q+1; q=findcomma(line,p); line[q]=0; e->ysize=strtol(line+p,0,10);
			p=q+1; q=findcomma(line,p); line[q]=0; e->year=strtol(line+p,0,10);
			p=q+1; q=findcomma(line,p); line[q]=0; e->month=strtol(line+p,0,10);
			p=q+1; q=findcomma(line,p); line[q]=0; e->date=strtol(line+p,0,10);
			p=q+1; q=findcomma(line,p); line[q]=0; e->hour=strtol(line+p,0,10);
			p=q+1; q=findcomma(line,p); line[q]=0; e->minute=strtol(line+p,0,10);
			p=q+1; q=findcomma(line,p); line[q]=0; e->hundredths=strtol(line+p,0,10);
			p=q+1; q=findcomma(line,p); line[q]=0; e->clicks=strtol(line+p,0,10);
		}
		fclose(f);
	}
}

static int ispuzzle(char *s) {
	if(strlen(s)>2 && !strncmp(s,"aka",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"has",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"hey",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"hit",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"kur",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"mas",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"min",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"nur",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"pic",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"sli",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"yaj",3)) return 1;
	return 0;
}

static void getpuzzleline(FILE *f,char *s,int maks) {
	int n;
	while(fgets(s,maks,f) && s[0]=='%');
	n=strlen(s);
	while(n && s[n-1]=='\n') s[--n]=0;
}

static int compcleanup(const void *A,const void *B) {
	const entry_t *a=A,*b=B;
	int c=strcmp(a->puzzleid,b->puzzleid);
	if(c) return c;
	if(a->ix<b->ix) return -1;
	return 1;
}

/* scan directory and add new puzzle files */
static void refreshcache() {
	static char path[MAXSTRING];
	static char line[MAXSTRING];
	static char temp[MAXSTRING];
	static char fn[MAXSTRING*2];
	FILE *f;
	dir_t dir;
	entry_t *e;
	int i,j;
	/* scan for new files */
	if(strlen(puzzlepath)+1>=MAXSTRING-1) error("path is too long");
	strcpy(path,puzzlepath);
	strcat(path,"*");
	findfirst(path,&dir);
	do {
		/* skip directories, including . .. */
		if(dir.dir>0) continue;
		if(!strcmp(dir.s,".") || !strcmp(dir.s,"..")) continue;
		/* skip file where three first characters != puzzle name */
		if(!ispuzzle(dir.s)) continue;
		if(strlen(dir.s)>=MAXID) continue;
		e=newentry();
		strcpy(e->puzzleid,dir.s);
		strcpy(e->display,"");
	} while(findnext(&dir));
	findclose(&dir);
	for(i=0;i<puzzlenum;i++) puzzlelist[i].ix=i;
	qsort(puzzlelist,puzzlenum,sizeof(entry_t),compcleanup);
	/* remove duplicates, keep first copy, as sort was stable */
	for(i=j=1;i<puzzlenum;i++) if(strcmp(puzzlelist[i-1].puzzleid,puzzlelist[i].puzzleid)) puzzlelist[j++]=puzzlelist[i];
	puzzlenum=j;
	/* get info about files with empty display string */
	for(i=0;i<puzzlenum;i++) if(!puzzlelist[i].display[0]) {
		strcpy(temp,puzzlelist[i].puzzleid+3);
		/* strip away extension */
		for(j=0;temp[j] && temp[j]!='.';j++);
		temp[j]=0;
		strcpy(fn,puzzlepath);
		strcat(fn,puzzlelist[i].puzzleid);
		f=fopen(fn,"r");
		e=puzzlelist+i;
		if(f) {
			/* first three non-comment lines are always puzzle name,
			   difficulty and size */
			getpuzzleline(f,line,MAXSTRING);
			if(strlen(line)+strlen(temp)+1>=MAXDISP-1) { puzzlenum--; goto close; }
			strcpy(e->display,line);
			strcat(e->display," ");
			strcat(e->display,temp);
			e->display[0]=toupper(e->display[0]);
			getpuzzleline(f,line,MAXSTRING);
			if(strlen(line)>=MAXDIFF-1) { puzzlenum--; goto close; }
			line[0]=toupper(line[0]);
			strcpy(e->difficulty,line);
			getpuzzleline(f,line,MAXSTRING);
			if(2!=sscanf(line,"%d %d",&e->xsize,&e->ysize)) { puzzlenum--; goto close; }
			e->year=e->month=e->date=e->hour=e->minute=e->hundredths=e->clicks=0;
		close:
			fclose(f);
		}
	}
}

static void writecache() {
	static char fn[MAXSTRING*2];
	FILE *f;
	int i;
	strcpy(fn,puzzlepath);
	strcat(fn,"cache.txt");
	f=fopen(fn,"w");
	if(!f) error("couldn't write cache file\n");
	for(i=0;i<puzzlenum;i++) fprintf(f,"%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		puzzlelist[i].puzzleid,
		puzzlelist[i].display,
		puzzlelist[i].difficulty,
		puzzlelist[i].xsize,
		puzzlelist[i].ysize,
		puzzlelist[i].year,
		puzzlelist[i].month,
		puzzlelist[i].date,
		puzzlelist[i].hour,
		puzzlelist[i].minute,
		puzzlelist[i].hundredths,
		puzzlelist[i].clicks);
	fclose(f);
}

static void scanfornewpuzzles() {
	refreshcache();
	writecache();
}

/* start of menu code */

/* types */
#define W_BUTTON 0
#define W_NOTHING 1
#define W_VSCROLL 2

#define A_LEFT 0
#define A_CENTER 1
#define A_RIGHT 2

#define MAXSTR 256
typedef struct {
	int x1,y1,x2,y2; /* bounding coordinates, all inclusive */
	int tx,ty; /* text offset from x1,y1 */
	int hasborder; /* 1: has border */
	Uint32 col,bkcol,border; /* textcolour, background, border */
	Uint32 hicol; /* alternative background colour for highlight (mouseover) */
	char s[MAXSTR];
	int adjust; /* left, center or right */
	int id;
	int type;
	int next; /* pointer to next linked widget, or -1 if none */
} widget_t;

#define MAXWIDGET 256
widget_t widget[MAXWIDGET];
int widgetnum;

#define TABLE_PUZZLE "Puzzle"
#define TABLE_DIFF "Difficulty"
#define TABLE_SIZE "Size"
#define TABLE_DATE "Date completed"
#define TABLE_SPENT "Time spent"
#define TABLE_CLICKS "Clicks"

static void addwidget(int x1,int y1,int x2,int y2,int tx,int ty,int hasborder,Uint32 col,Uint32 bkcol,
	Uint32 border,Uint32 hicol,char *s,int adjust,int id,int type,int next) {
	widget[widgetnum].x1=x1;
	widget[widgetnum].y1=y1;
	widget[widgetnum].x2=x2;
	widget[widgetnum].y2=y2;
	widget[widgetnum].tx=tx;
	widget[widgetnum].ty=ty;
	widget[widgetnum].hasborder=hasborder;
	widget[widgetnum].col=col;
	widget[widgetnum].bkcol=bkcol;
	widget[widgetnum].border=border;
	widget[widgetnum].hicol=hicol;
	strcpy(widget[widgetnum].s,s);
	widget[widgetnum].adjust=adjust;
	widget[widgetnum].id=id;
	widget[widgetnum].type=type;
	widget[widgetnum].next=next;
	widgetnum++;
}

static void drawwidget(widget_t *w,int mouse) {
	int v;
	if(w->hasborder) {
		drawrectangle32(w->x1+1,w->y1+1,w->x2-1,w->y2-1,mouse?w->hicol:w->bkcol);
		drawhorizontalline32(w->x1,w->x2,w->y1,w->border);
		drawhorizontalline32(w->x1,w->x2,w->y2,w->border);
		drawverticalline32(w->x1,w->y1+1,w->y2-1,w->border);
		drawverticalline32(w->x2,w->y1+1,w->y2-1,w->border);
	} else drawrectangle32(w->x1,w->y1,w->x2,w->y2,mouse?w->hicol:w->bkcol);
	if(w->s[0]) {
		if(w->adjust==A_LEFT) sdl_font_printf(screen,font,w->x1+w->tx,w->y1+w->ty,w->col,w->col,"%s",w->s);
		else {
			v=sdl_font_width(font,w->s);
			if(w->adjust==A_CENTER) sdl_font_printf(screen,font,
				w->x1+w->tx+(w->x2-w->x1-v-1)/2,w->y1+w->ty,w->col,w->col,"%s",w->s);
			else sdl_font_printf(screen,font,
				w->x2-w->tx-1-v,w->y1+w->ty,w->col,w->col,"%s",w->s);
		}
	}
}

static void placement() {
	int h,solved=0,i,w,neww;
	int xx[8],yy;
	char s[MAXSTR];
	startx=10;
	starty=10;
	logox=0;
	logoy=font->height+5;
	buttonwidth=200;
	buttonheight=font->height+6;
	buttony=10;
	bottomy=font->height+6;
	h=buttony+buttonheight;
	bottomair=20;
	rightair=10;
	numentries=(resy-starty-logoy-font->height-1*(bottomy+font->height)-5-bottomair)/(font->height+2);
	tablex=buttonwidth+buttonheight;
	tablebottomy=starty+logoy+font->height+bottomy+numentries*(font->height+2);
	widgetnum=0;
	addwidget(tablex,starty,resx-1,starty+logoy-1,0,0,0,
		BLACK32,WHITE32,WHITE32,WHITE32,"The Puzzle Game",A_LEFT,-1,W_NOTHING,-1);
	addwidget(startx,starty+logoy,startx+buttonwidth-1,starty+logoy+buttonheight-1,
		3,3,1,BLACK32,WHITE32,BLACK32,YELLOW32,"(P)lay",A_CENTER,-1,W_BUTTON,-1);
	addwidget(startx,starty+logoy+h,startx+buttonwidth-1,starty+logoy+buttonheight-1+h,
		3,3,1,BLACK32,GRAY32,BLACK32,GRAY32,"Credits",A_CENTER,-1,W_NOTHING,-1);
	addwidget(startx,starty+logoy+2*h,startx+buttonwidth-1,starty+logoy+buttonheight-1+2*h,
		3,3,1,BLACK32,GRAY32,BLACK32,GRAY32,"How to play",A_CENTER,-1,W_NOTHING,-1);
	addwidget(startx,starty+logoy+3*h,startx+buttonwidth-1,starty+logoy+buttonheight-1+3*h,
		3,3,1,BLACK32,GRAY32,BLACK32,GRAY32,"Achievements",A_CENTER,-1,W_NOTHING,-1);
	addwidget(startx,starty+logoy+4*h,startx+buttonwidth-1,starty+logoy+buttonheight-1+4*h,
		3,3,1,BLACK32,GRAY32,BLACK32,GRAY32,"Options",A_CENTER,-1,W_NOTHING,-1);
	addwidget(startx,tablebottomy-buttonheight+1-h,startx+buttonwidth-1,tablebottomy-h,
		3,3,1,BLACK32,WHITE32,BLACK32,YELLOW32,"(S)can for new puzzles",A_CENTER,-1,W_BUTTON,-1);
	addwidget(startx,tablebottomy-buttonheight+1,startx+buttonwidth-1,tablebottomy,
		3,3,1,BLACK32,WHITE32,BLACK32,YELLOW32,"(Q)uit",A_CENTER,-1,W_BUTTON,-1);
	addwidget(startx,tablebottomy+1+bottomair,startx+buttonwidth,tablebottomy+font->height+2+bottomair,
		2,2,0,BLACK32,WHITE32,BLACK32,WHITE32,VERSION_STRING,A_LEFT,-1,W_NOTHING,-1);
	for(i=0;i<puzzlenum;i++) if(puzzlelist[i].year) solved++;
	sprintf(s,"%d of %d puzzles solved",solved,puzzlenum);
	addwidget(tablex,tablebottomy+1+bottomair,resx-1,tablebottomy+bottomair+font->height+2,
		2,2,0,BLACK32,WHITE32,BLACK32,WHITE32,s,A_LEFT,-1,W_NOTHING,-1);
	/* table - everything except contents */
	xx[0]=tablex;
	xx[7]=resx-rightair;
	xx[6]=xx[7]-20;
	xx[5]=xx[6]-6-sdl_font_width(font,TABLE_CLICKS);
	xx[4]=xx[5]-6-sdl_font_width(font,TABLE_SPENT);
	xx[3]=xx[4]-6-sdl_font_width(font,TABLE_DATE);
	xx[2]=xx[3]-6-sdl_font_width(font,"999x999");
	w=sdl_font_width(font,TABLE_DIFF);
	for(i=0;i<puzzlenum;i++) {
		neww=sdl_font_width(font,puzzlelist[i].difficulty);
		if(w<neww) w=neww;
	}
	xx[1]=xx[2]-7-w;
	yy=starty+logoy;
	addwidget(xx[0],yy,xx[7],yy,0,0,0,BLACK32,BLACK32,BLACK32,BLACK32,"",0,-1,W_NOTHING,-1);
	for(i=0;i<8;i++) addwidget(xx[i],yy,xx[i],tablebottomy,0,0,0,BLACK32,BLACK32,BLACK32,BLACK32,"",0,-1,W_NOTHING,-1);
	addwidget(xx[0],tablebottomy,xx[7],tablebottomy,0,0,0,BLACK32,BLACK32,BLACK32,BLACK32,"",0,-1,W_NOTHING,-1);
	/* column headers */
	addwidget(xx[0]+1,yy+1,xx[1]-1,yy+1+buttonheight-1,2,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_PUZZLE,A_LEFT,-1,W_BUTTON,-1);
	addwidget(xx[1]+1,yy+1,xx[2]-1,yy+1+buttonheight-1,2,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_DIFF,A_LEFT,-1,W_BUTTON,-1);
	addwidget(xx[2]+1,yy+1,xx[3]-1,yy+1+buttonheight-1,2,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_SIZE,A_LEFT,-1,W_BUTTON,-1);
	addwidget(xx[3]+1,yy+1,xx[4]-1,yy+1+buttonheight-1,2,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_DATE,A_LEFT,-1,W_BUTTON,-1);
	addwidget(xx[4]+1,yy+1,xx[5]-1,yy+1+buttonheight-1,2,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_SPENT,A_RIGHT,-1,W_BUTTON,-1);
	addwidget(xx[5]+1,yy+1,xx[6]-1,yy+1+buttonheight-1,2,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_CLICKS,A_RIGHT,-1,W_BUTTON,-1);
	yy+=buttonheight+1;
	addwidget(xx[0],yy,xx[7],yy,0,0,0,BLACK32,BLACK32,BLACK32,BLACK32,"",0,-1,W_NOTHING,-1);
	yy++;
}

static void drawmenu() {
	int i;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
	if(resx<830 || resy<300) {
		clear32(RED32);
		sdl_font_printf(screen,font,0,0,BLACK32,BLACK32,"please resize window");
	} else {
		clear32(WHITE32);
		for(i=0;i<widgetnum;i++) drawwidget(widget+i,0);
	}
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
}

void menu() {
	int event;
	loadcache();
	placement();
	drawmenu();
  do {
    event=getevent();
    switch(event) {
    case EVENT_RESIZE:
			placement();
      drawmenu();
    }
  } while(event!=EVENT_QUIT && !keys[SDLK_ESCAPE]);
}
