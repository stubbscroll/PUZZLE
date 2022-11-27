#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "SDL/SDL.h"
#include "dir.h"
#include "puzzle.h"
#include "sdlfont.h"

#define MAXID 16
#define MAXDISP 32
#define MAXDIFF 32

/* minimum resolution for menu */
#define MINRESX 832
#define MINRESY 300

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

static int sortcolumn=0; /* id, diff, date, time, clicks */
static int sortorder=0; /* 0 increasing, 1 decreasing */

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
	if(strlen(s)>2 && !strncmp(s,"num",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"nur",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"pic",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"sli",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"yaj",3)) return 1;
	if(strlen(s)>2 && !strncmp(s,"mor",3)) return 1;
	return 0;
}

static void getpuzzleline(FILE *f,char *s,int maks) {
	int n;
	while(fgets(s,maks,f) && s[0]=='%');
	n=strlen(s);
	while(n && (s[n-1]=='\n' || s[n-1]=='\r')) s[--n]=0;
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
#ifdef _WIN32
	strcat(path,"*");
#endif
	if(!findfirst(path,&dir)) error("bad path, no files found\n");
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

#define MAXWIDGET 1024
static widget_t widget[MAXWIDGET];
static int widgetnum;

static int selected=0; /* index of selected entry in list */
static int ypos=0; /* index of topmost shown entry, cannot exceed puzzlenum-numentries */
static int widgetix; /* index to first widget containing table element */

static int focusid; /* id of gui element in focus */
static int focusix; /* index to widget of above element */

static int scrollbarix; /* index to scrollbar */

#define TABLE_PUZZLE "Puzzle"
#define TABLE_DIFF "Difficulty"
#define TABLE_SIZE "Size"
#define TABLE_DATE "Date completed"
#define TABLE_SPENT "Time spent"
#define TABLE_CLICKS "Clicks"

static void addwidget(int x1,int y1,int x2,int y2,int tx,int ty,int hasborder,Uint32 col,Uint32 bkcol,
	Uint32 border,Uint32 hicol,char *s,int adjust,int id,int type,int next) {
	if(widgetnum>=MAXWIDGET) error("too many widgets on-screen");
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

static char *shortenleft(sdl_font *font,char *s,int avail) {
	static char t[65536];
	int endw=sdl_font_width(font,"...")-1,i,p=0;
	/* be sneaky and assume that the hidden value padw in sdlfont is always 1 */
	for(i=0;s[i];i++) if((uchar)s[i]>31) {
		if(endw+font->w[(uchar)s[i]-32]+1<=avail) t[p++]=s[i],endw+=1+font->w[(uchar)s[i]-32];
		else break;
	}
	t[p++]='.'; t[p++]='.'; t[p++]='.';
	t[p]=0;
	return t;
}

static char *shortenright(sdl_font *font,char *s,int avail) {
	static char t[65536];
	int endw=sdl_font_width(font,"...")-1,p=65535,i;
	t[p--]=0;
	for(i=strlen(s)-1;i>=0;i--) if((uchar)s[i]>31) {
		if(endw+font->w[(uchar)s[i]-32]+1<=avail) t[p--]=s[i],endw+=1+font->w[(uchar)s[i]-32];
		else break;
	}
	t[p--]='.'; t[p--]='.'; t[p]='.'; 
	return t+p;
}

static void drawwidget(widget_t *w,int mouse) {
	int v,avail,barrange,listrange,top,bottom;
	char *s=w->s;
	if(w->type==W_VSCROLL && puzzlenum>numentries) {
		drawrectangle32(w->x1,w->y1,w->x2,w->y2,mouse?w->hicol:w->bkcol);
		barrange=w->y2-w->y1;
		listrange=puzzlenum-numentries+1;
		top=w->y1+ypos*barrange*3/listrange/4;
		bottom=top+barrange-barrange*3/4;
		if(bottom>=w->y2) bottom=w->y2;
		drawrectangle32(w->x1,top,w->x2,bottom,mouse?w->border:w->col);
	} else if(w->hasborder) {
		drawrectangle32(w->x1+1,w->y1+1,w->x2-1,w->y2-1,mouse?w->hicol:w->bkcol);
		drawhorizontalline32(w->x1,w->x2,w->y1,w->border);
		drawhorizontalline32(w->x1,w->x2,w->y2,w->border);
		drawverticalline32(w->x1,w->y1+1,w->y2-1,w->border);
		drawverticalline32(w->x2,w->y1+1,w->y2-1,w->border);
	} else drawrectangle32(w->x1,w->y1,w->x2,w->y2,mouse?w->hicol:w->bkcol);
	if(w->s[0]) {
		avail=w->x2-w->x1-2*w->tx-1;
		if(w->adjust==A_LEFT) {
			if(sdl_font_width(font,s)>avail) s=shortenleft(font,s,avail);
			sdl_font_printf(screen,font,w->x1+w->tx,w->y1+w->ty,w->col,w->col,"%s",s);
		}
		else {
			if(w->adjust==A_CENTER) {
				if(sdl_font_width(font,s)>avail) s=shortenleft(font,s,avail);
				v=sdl_font_width(font,s);
				sdl_font_printf(screen,font,
					w->x1+w->tx+(w->x2-w->x1-v-1)/2,w->y1+w->ty,w->col,w->col,"%s",s);
			} else {
				if(sdl_font_width(font,s)>avail) s=shortenright(font,s,avail);
				v=sdl_font_width(font,s);
				sdl_font_printf(screen,font,
					w->x2-w->tx-1-v,w->y1+w->ty,w->col,w->col,"%s",s);
			}
		}
	}
}

static void updatetablecol(int n) {
	int i=n,k=widgetix+n*6,l;
	for(l=0;l<6;l++) {
		widget[k+l].bkcol=(i==selected)?GREEN32:WHITE32;
		widget[k+l].hicol=(i==selected)?BLUE32:YELLOW32;
	}
}

static void updatetable(int n) {
	int j=ypos+n,k=widgetix+n*6;
	entry_t *e;
	e=puzzlelist+j;
	strcpy(widget[k].s,e->display);
	strcpy(widget[k+1].s,e->difficulty);
	sprintf(widget[k+2].s,"%dx%d",e->xsize,e->ysize);
	if(e->year) {
		sprintf(widget[k+3].s,"%04d-%02d-%02d %02d:%02d",
			e->year,e->month,e->date,e->hour,e->minute);
		if(e->hundredths<6000) sprintf(widget[k+4].s,"%d:%02d",e->hundredths/100,e->hundredths%100);
		else if(e->hundredths<6000*60) sprintf(widget[k+4].s,"%d.%02d:%02d",e->hundredths/6000,e->hundredths/100%60,e->hundredths%100);
		else sprintf(widget[k+4].s,"%d.%02d.%02d:%02d",e->hundredths/6000/60,e->hundredths/6000%60,e->hundredths/100%60,e->hundredths%100);
		sprintf(widget[k+5].s,"%d",e->clicks);
	} else widget[k+3].s[0]=widget[k+4].s[0]=widget[k+5].s[0]=0;
	/* update colours for selected entry */
	updatetablecol(n);
}

/* place widgets. list of ids:
   0: play
	 1: credits
	 2: how to play
	 3: achievements
	 4: options
	 48: scan for new puzzles
	 49: quit
	 50-55: column headers
	 100-105: row 0
	 106-111: row 1
	 ...
	 100+i*6-105+i*6: row i
*/

static void placement() {
	int h,solved=0,i,w,neww,cur;
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
	numentries=(resy-starty-logoy-1*font->height-1*(buttonheight)-5-bottomair)/(font->height+2);
	tablex=buttonwidth+buttonheight;
	tablebottomy=starty+logoy+buttonheight+numentries*(font->height+2)+2;
	widgetnum=0;
	if(numentries>puzzlenum) numentries=puzzlenum;
	/* take care or cursor and list position */
	if(selected>=numentries) {
		logprintf("selected too large: %d !<= %d (ypos %d)\n",selected,numentries);
		cur=selected-numentries+1;
		ypos+=cur;
		selected-=cur;
		logprintf("new selected %d ypos %d\n",selected,ypos);
	}
	if(ypos+numentries>puzzlenum) {
		cur=puzzlenum-numentries-1;
		logprintf("ypos too large: %d, diff %d (sel %d)\n",ypos,cur,selected);
		selected+=ypos-cur-1;
		ypos=cur+1;
		logprintf("new ypos %d selected %d numentries %d\n",ypos,selected,numentries);
	}
	addwidget(tablex,starty,resx-1,starty+logoy-1,0,0,0,
		BLACK32,WHITE32,WHITE32,WHITE32,"The Puzzle Game",A_LEFT,-1,W_NOTHING,-1);
	addwidget(startx,starty+logoy,startx+buttonwidth-1,starty+logoy+buttonheight-1,
		3,3,1,BLACK32,WHITE32,BLACK32,YELLOW32,"(P)lay",A_CENTER,0,W_BUTTON,-1);
	/* remove nonfunctional buttons until they actually function */
/*	addwidget(startx,starty+logoy+h,startx+buttonwidth-1,starty+logoy+buttonheight-1+h,
		3,3,1,BLACK32,GRAY32,BLACK32,GRAY32,"Credits",A_CENTER,1,W_NOTHING,-1);
	addwidget(startx,starty+logoy+2*h,startx+buttonwidth-1,starty+logoy+buttonheight-1+2*h,
		3,3,1,BLACK32,GRAY32,BLACK32,GRAY32,"How to play",A_CENTER,2,W_NOTHING,-1);
	addwidget(startx,starty+logoy+3*h,startx+buttonwidth-1,starty+logoy+buttonheight-1+3*h,
		3,3,1,BLACK32,GRAY32,BLACK32,GRAY32,"Achievements",A_CENTER,3,W_NOTHING,-1);
	addwidget(startx,starty+logoy+4*h,startx+buttonwidth-1,starty+logoy+buttonheight-1+4*h,
		3,3,1,BLACK32,GRAY32,BLACK32,GRAY32,"Options",A_CENTER,4,W_NOTHING,-1);
*/
	addwidget(startx,tablebottomy-buttonheight+1-h,startx+buttonwidth-1,tablebottomy-h,
		3,3,1,BLACK32,WHITE32,BLACK32,YELLOW32,"(S)can for new puzzles",A_CENTER,48,W_BUTTON,-1);
	addwidget(startx,tablebottomy-buttonheight+1,startx+buttonwidth-1,tablebottomy,
		3,3,1,BLACK32,WHITE32,BLACK32,YELLOW32,"(Q)uit",A_CENTER,49,W_BUTTON,-1);
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
	xx[5]=xx[6]-5-sdl_font_width(font,"9999999");
	xx[4]=xx[5]-5-sdl_font_width(font,"999.59.59:99");
	xx[3]=xx[4]-5-sdl_font_width(font,"0000-10-00 00:00 ");
	xx[2]=xx[3]-5-sdl_font_width(font,"999x999");
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
	addwidget(xx[0]+1,yy+1,xx[1]-1,yy+1+buttonheight-1,1,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_PUZZLE,A_LEFT,50,W_BUTTON,-1);
	addwidget(xx[1]+1,yy+1,xx[2]-1,yy+1+buttonheight-1,1,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_DIFF,A_LEFT,51,W_BUTTON,-1);
	addwidget(xx[2]+1,yy+1,xx[3]-1,yy+1+buttonheight-1,1,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_SIZE,A_CENTER,52,W_BUTTON,-1);
	addwidget(xx[3]+1,yy+1,xx[4]-1,yy+1+buttonheight-1,1,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_DATE,A_LEFT,53,W_BUTTON,-1);
	addwidget(xx[4]+1,yy+1,xx[5]-1,yy+1+buttonheight-1,1,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_SPENT,A_RIGHT,54,W_BUTTON,-1);
	addwidget(xx[5]+1,yy+1,xx[6]-1,yy+1+buttonheight-1,1,2,0,
		BLACK32,WHITE32,BLACK32,YELLOW32,TABLE_CLICKS,A_RIGHT,55,W_BUTTON,-1);
	yy+=buttonheight+1;
	addwidget(xx[0],yy,xx[7],yy,0,0,0,BLACK32,BLACK32,BLACK32,BLACK32,"",0,-1,W_NOTHING,-1);
	yy++;
	widgetix=widgetnum;
	for(i=0;i<numentries;i++) {
		/* create empty entries for each cell */
		cur=widgetnum;
		addwidget(xx[0]+1,yy,xx[1]-1,yy+font->height+1,1,1,0,BLACK32,WHITE32,BLACK32,YELLOW32,"",A_LEFT,100+i,W_BUTTON,cur+1);
		addwidget(xx[1]+1,yy,xx[2]-1,yy+font->height+1,1,1,0,BLACK32,WHITE32,BLACK32,YELLOW32,"",A_LEFT,100+i,W_BUTTON,cur+2);
		addwidget(xx[2]+1,yy,xx[3]-1,yy+font->height+1,1,1,0,BLACK32,WHITE32,BLACK32,YELLOW32,"",A_CENTER,100+i,W_BUTTON,cur+3);
		addwidget(xx[3]+1,yy,xx[4]-1,yy+font->height+1,1,1,0,BLACK32,WHITE32,BLACK32,YELLOW32,"",A_LEFT,100+i,W_BUTTON,cur+4);
		addwidget(xx[4]+1,yy,xx[5]-1,yy+font->height+1,1,1,0,BLACK32,WHITE32,BLACK32,YELLOW32,"",A_RIGHT,100+i,W_BUTTON,cur+5);
		addwidget(xx[5]+1,yy,xx[6]-1,yy+font->height+1,1,1,0,BLACK32,WHITE32,BLACK32,YELLOW32,"",A_RIGHT,100+i,W_BUTTON,cur);
		yy+=font->height+2;
		updatetable(i);
	}
	scrollbarix=widgetnum;
	addwidget(xx[6]+1,starty+logoy+2*buttonheight+3,xx[7]-1,tablebottomy-buttonheight-2,0,0,0,WHITE32,GRAY32,YELLOW32,GRAY32,"",0,80,W_VSCROLL,-1);
	addwidget(xx[6]+1,starty+logoy+2*buttonheight+2,xx[7]-1,starty+logoy+2*buttonheight+2,0,0,0,BLACK32,BLACK32,BLACK32,BLACK32,"",0,-1,W_NOTHING,-1);
	addwidget(xx[6]+1,tablebottomy-buttonheight-1,xx[7]-1,tablebottomy-buttonheight-1,0,0,0,BLACK32,BLACK32,BLACK32,BLACK32,"",0,-1,W_NOTHING,-1);
	addwidget(xx[6]+1,starty+logoy+buttonheight+2,xx[7]-1,starty+logoy+2*buttonheight+1,1,1,0,BLACK32,WHITE32,BLACK32,YELLOW32,"^",A_CENTER,81,W_BUTTON,-1);
	addwidget(xx[6]+1,tablebottomy-buttonheight,xx[7]-1,tablebottomy-1,1,1,0,BLACK32,WHITE32,BLACK32,YELLOW32,"v",A_CENTER,82,W_BUTTON,-1);
}

static void drawmenu() {
	int i;
  if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
	if(resx<MINRESX || resy<MINRESY) {
		clear32(RED32);
		sdl_font_printf(screen,font,0,0,BLACK32,BLACK32,"please resize window");
	} else {
		clear32(WHITE32);
		for(i=0;i<widgetnum;i++) drawwidget(widget+i,0);
	}
  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen,0,0,resx,resy);
	focusid=focusix=-1;
}

static void redrawtableline(int n) {
	int ix=n*6+widgetix,j;
	for(j=0;j<6;j++) drawwidget(widget+j+ix,0);
	drawwidget(widget+scrollbarix,0);
}

static void redrawtable() {
	int i;
	for(i=0;i<numentries;i++) redrawtableline(i);
}

static void play(int n) {
	static char fn[MAXSTRING];
	int improve=0;
	time_t tid;
	struct tm *t;
	entry_t *e;
	strcpy(fn,puzzlepath);
	strcat(fn,puzzlelist[n].puzzleid);
	launch(fn,0);
	memset(keys,0,sizeof(keys));
	/* level is solved using no hints: success */
	if(timespent && !usedhint) {
		e=puzzlelist+n;
		if(!e->hundredths || e->hundredths>timespent) e->hundredths=timespent,improve=1;
		if(!e->clicks || e->clicks>numclicks) e->clicks=numclicks,improve=1;
		if(improve) {
			time(&tid);
			t=localtime(&tid);
			e->year=t->tm_year+1900;
			e->month=t->tm_mon+1;
			e->date=t->tm_mday;
			e->hour=t->tm_hour;
			e->minute=t->tm_min;
			writecache();
		}
	}
	if(resx>=MINRESX && resy>=MINRESY) placement();
	else widgetnum=0;
	drawmenu();
}

static void scanforpuzzlesgui() {
	messagebox(0,"Wait, scanning for new puzzles");
	scanfornewpuzzles();
	selected=ypos=0;
	placement();
	drawmenu();
}

static void updatetworowsintable(int oldsel,int selected) {
	if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
	updatetablecol(oldsel);
	updatetablecol(selected);
	redrawtableline(oldsel);
	redrawtableline(selected);
	if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	/* laziness: refresh entire table. if this leads to performance problems,
		 only update the two actual lines */
	SDL_UpdateRect(screen,tablex,starty+logoy,resx-tablex,tablebottomy+1-starty-logoy);
}

#define GOUP if(selected+ypos) { --selected; if(selected<0) selected=0,ypos--,scroll=1; }
#define GODOWN if(selected+ypos+1<puzzlenum) { ++selected; if(selected==numentries) selected--,ypos++,scroll=1; }
static void processkeydown(int key) {
	int scroll=0,i,oldsel=selected;
	switch(key) {
	case SDLK_UP:
		GOUP
		break;
	case SDLK_DOWN:
		GODOWN
		break;
	case SDLK_PAGEUP:
	case SDLK_KP9:
		for(i=1;i<numentries;i++) GOUP
		break;
	case SDLK_PAGEDOWN:
	case SDLK_KP3:
		for(i=1;i<numentries;i++) GODOWN
		break;
	case SDLK_p:
	case SDLK_RETURN:
		play(selected+ypos);
		break;
	case SDLK_HOME:
	case SDLK_KP7:
		if(ypos) selected=ypos=0,scroll=1;
		if(selected) selected=0;
		break;
	case SDLK_END:
	case SDLK_KP1:
		if(ypos<puzzlenum-numentries) ypos=puzzlenum-numentries,scroll=1;
		if(selected<numentries-1) selected=numentries-1;
		break;
	case SDLK_s:
		scanforpuzzlesgui();
		break;
	}
	if(scroll) {
		if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
		for(i=0;i<numentries;i++) updatetable(i);
		redrawtable();
		if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		SDL_UpdateRect(screen,tablex,starty+logoy,resx-tablex,tablebottomy+1-starty-logoy);
	} else if(selected!=oldsel) {
		updatetworowsintable(oldsel,selected);
	}
}

/* find id of first element in a chain of gui elements */
static int findfirstinchain(int ix) {
	int min=ix,old=ix;
	ix=widget[ix].next;
	if(ix<0) return min;
	while(ix>-1 && old!=ix) {
		if(min>ix) min=ix;
		ix=widget[ix].next;
	}
	return min;
}

/* redraw an entire chain of gui elements */
/* must never be called with invalid ix like -1 */
static void drawguielements(int ix,int mouse) {
	int old=ix,minx=widget[ix].x1,miny=widget[ix].y1,maxx=widget[ix].x2;
	int maxy=widget[ix].y2;
	if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
	drawwidget(widget+ix,mouse);
	ix=widget[ix].next;
	while(ix>-1 && ix!=old) {
		drawwidget(widget+ix,mouse);
		if(minx>widget[ix].x1) minx=widget[ix].x1;
		if(miny>widget[ix].y1) miny=widget[ix].y1;
		if(maxx<widget[ix].x2) maxx=widget[ix].x2;
		if(maxy<widget[ix].y2) maxy=widget[ix].y2;
		ix=widget[ix].next;
	}
	if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	SDL_UpdateRect(screen,minx,miny,maxx-minx+1,maxy-miny+1);
}

static void processmousemotion() {
	int i,x=event_mousex,y=event_mousey,x1,y1;
	int barrange,listrange,top,bottom,newypos,dy;
	widget_t *w;
	if(resx<MINRESX) return;
	for(i=0;i<widgetnum;i++) {
		w=widget+i;
		if(x>=w->x1 && y>=w->y1 && x<=w->x2 && y<=w->y2) {
			if(w->id==focusid) {
				/* we might be inside scrollbar here */
				if(w->type==W_VSCROLL && puzzlenum>numentries && mousebuttons[0]) {
					x1=event_mousefromx; y1=event_mousefromy;
					/* calculate coordinates of scrollbar */
					barrange=w->y2-w->y1;
					listrange=puzzlenum-numentries+1;
					top=w->y1+ypos*barrange*3/listrange/4;
					bottom=top+barrange-barrange*3/4;
					if(bottom>=w->y2) bottom=w->y2;
					if(x1>=w->x1 && x1<=w->x2 && y1>=top && y1<=bottom) {
						/* we're inside scroll bar box, change ypos */
						dy=y-y1;
						newypos=ypos+dy*(puzzlenum-numentries+1)/(barrange*3/4);
						if(newypos==ypos) {
							if(dy<0) newypos--;
							else newypos++;
						}
						ypos=newypos;
						if(ypos<0) ypos=0;
						if(ypos>puzzlenum-numentries) ypos=puzzlenum-numentries;
						if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
						for(i=0;i<numentries;i++) updatetable(i);
						redrawtable();
						if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
						SDL_UpdateRect(screen,tablex,starty+logoy,resx-tablex,tablebottomy+1-starty-logoy);
					}
				}
				return;
			}
			/* draw previous element without focus */
			if(focusix>-1) drawguielements(focusix,0);
			focusid=w->id;
			focusix=findfirstinchain(i);
			if(focusid>-1) drawguielements(i,1);
			return;
		}
	}
	if(focusix>-1) drawguielements(focusix,0);
	focusix=focusid=-1;
}

/* set focus on element in table with the given ix */
static void setfocus(int ix) {
	int i;
	for(i=0;i<puzzlenum;i++) if(ix==puzzlelist[i].ix) break;
	if(i==puzzlenum) error("internal error, ix not found in setfocus");
	ypos=i;
	selected=0;
	while(ypos+numentries>puzzlenum) selected++,ypos--;
	while(selected+selected<numentries && ypos+numentries<puzzlenum && ypos>0)
		selected++,ypos--;
}

static int compcustomlist(const void *A,const void *B) {
	const entry_t *a=A,*b=B;
	int sign=sortorder?-1:1,r,s;
	switch(sortcolumn) {
	case 0: /* puzzle id */
		r=strcmp(a->puzzleid,b->puzzleid);
		if(r) return r*sign;
		break;
	case 1: /* difficulty */
		r=getnumericdiff(a->difficulty);
		s=getnumericdiff(b->difficulty);
		if(r<s) return -sign;
		if(r>s) return sign;
		break;
	case 2: /* size */
		if(a->xsize*a->ysize<b->xsize*b->ysize) return -sign;
		if(a->xsize*a->ysize>b->xsize*b->ysize) return sign;
		if(a->xsize<b->xsize) return -sign;
		if(a->xsize>b->xsize) return sign;
		break;
	case 3: /* date solved */
		if(a->year<b->year) return -sign;
		if(a->year>b->year) return sign;
		if(a->month<b->month) return -sign;
		if(a->month>b->month) return sign;
		if(a->date<b->date) return -sign;
		if(a->date>b->date) return sign;
		if(a->hour<b->hour) return -sign;
		if(a->hour>b->hour) return sign;
		if(a->minute<b->minute) return -sign;
		if(a->minute>b->minute) return sign;
		break;
	case 4: /* time solved */
		if(a->hundredths<b->hundredths) return -sign;
		if(a->hundredths>b->hundredths) return sign;
		break;
	case 5: /* clicks */
		if(a->clicks<b->clicks) return -sign;
		if(a->clicks>b->clicks) return sign;
	}
	if(a->ix<b->ix) return -1;
	return 1;
}

static void processmousedown() {
	int x=event_mousex,y=event_mousey,oldsel=selected,i,curix;
	int scroll=0;
	int barrange,listrange,top,bottom;
	static Uint32 lasttick=0;
	Uint32 tick;
	widget_t *w;
	if(resx<MINRESX) return;
	if(mousebuttons[0]) for(i=0;i<widgetnum;i++) {
		w=widget+i;
		if(x>=w->x1 && y>=w->y1 && x<=w->x2 && y<=w->y2 && w->id>-1) {
			if(w->type==W_BUTTON) {
				/* we clicked on something */
				if(w->id==0) play(selected+ypos);
				/* TODO 1 (credits) 2 (how to play) 3 (achievements) 4 (options) */
				else if(w->id==48) scanforpuzzlesgui();
				else if(w->id==49) keys[SDLK_ESCAPE]=1;
				else if(w->id>49 && w->id<56) {
					if(w->id-50==sortcolumn) sortorder^=1;
					else sortorder=0,sortcolumn=w->id-50;
					for(i=0;i<puzzlenum;i++) puzzlelist[i].ix=i;
					curix=selected+ypos;
					qsort(puzzlelist,puzzlenum,sizeof(entry_t),compcustomlist);
					setfocus(curix);
					if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
					for(i=0;i<numentries;i++) updatetable(i);
					redrawtable();
					if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
					SDL_UpdateRect(screen,tablex,starty+logoy,resx-tablex,tablebottomy+1-starty-logoy);
				} else if(w->id>99) {
					selected=w->id-100;
					/* check for doubleclick */
					tick=SDL_GetTicks();
					if(selected==oldsel) {
						if(tick-lasttick<500) play(selected+ypos);
						lasttick=tick;
						return;
					}
					lasttick=tick;
					if(selected!=oldsel) updatetworowsintable(oldsel,selected);
				} else if(w->id==81) {
					/* scroll bar button up */
					if(ypos) ypos--,scroll=1;
					goto updategraphics;
				} else if(w->id==82) {
					/* scroll bar button down */
					if(ypos<puzzlenum-numentries) ypos++,scroll=1;
					goto updategraphics;
				}
				return;
			} else if(w->type==W_VSCROLL) {
				/* check if we clicked above or below scroll bar box */
				barrange=w->y2-w->y1;
				listrange=puzzlenum-numentries+1;
				top=w->y1+ypos*barrange*3/listrange/4;
				bottom=top+barrange-barrange*3/4;
				if(bottom>=w->y2) bottom=w->y2;
				if(y<top) {
					for(i=1;i<numentries;i++) if(ypos) ypos--,scroll=1; else break;
				} else if(y>bottom) {
					for(i=1;i<numentries;i++) if(ypos<puzzlenum-numentries) ypos++,scroll=1; else break;
				}
				goto updategraphics;
			}
		}
	} else if(mousebuttons[4]) {
		for(i=1;i<numentries;i++) if(ypos) ypos--,scroll=1; else break;
	} else if(mousebuttons[3]) {
		for(i=1;i<numentries;i++) if(ypos<puzzlenum-numentries) ypos++,scroll=1; else break;
	}
updategraphics:
	if(scroll) {
		if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
		for(i=0;i<numentries;i++) updatetable(i);
		redrawtable();
		if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		SDL_UpdateRect(screen,tablex,starty+logoy,resx-tablex,tablebottomy+1-starty-logoy);
	} else if(selected!=oldsel) {
		updatetworowsintable(oldsel,selected);
	}
}
#undef GOUP
#undef GODOWN

void menu() {
	int event;
	loadcache();
	if(!puzzlenum) {
		scanfornewpuzzles();
		if(!puzzlenum) error("error, no puzzles found");
	}
	placement();
	drawmenu();
	do {
		event=getevent();
		switch(event) {
		case EVENT_RESIZE:
			placement();
			drawmenu();
			break;
		case EVENT_MOUSEDOWN:
			processmousedown();
			break;
		case EVENT_MOUSEMOTION:
			processmousemotion();
			break;
		default:
			if(event>=EVENT_KEYDOWN && event<EVENT_KEYUP) {
				processkeydown(event-EVENT_KEYDOWN);
			}
		}
	} while(event!=EVENT_QUIT && !keys[SDLK_ESCAPE] && !keys[SDLK_q]);
}
