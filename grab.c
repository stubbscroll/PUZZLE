#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <curl/curl.h>
#include <curl/easy.h>

#define STR 16384
#define BUF 5000000
char buffer[BUF];
int bptr;
char buf2[BUF];
int diff[1000000];
char diffstrs[12][32]={
  "trivial",    /*  leicht */
  "very easy",  /*  1 */
  "easy",       /*  2 */
  "easy+",      /*  3 */
  "medium-",    /*  4 */
  "medium",     /*  5 */
  "hard-",      /*  6 */
  "hard",       /*  7 */
  "very hard",  /*  8 */
  "extreme",    /*  schwer */
};

char *writeto;

size_t webline(void *ptr,size_t size,size_t n,void *userdata) {
  int i;
  char *t=(char *)ptr;
  for(i=0;t[i];i++) writeto[bptr++]=t[i];
  writeto[bptr]=0;
  return n;
}

void loadwebpage(char *url,char *buf) {
  CURL *curl=curl_easy_init();
  CURLcode res;
  writeto=buf;
  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,webline);
  bptr=0;
  res=curl_easy_perform(curl);
  curl_easy_cleanup(curl);
}

#define MAXP 10000
int num;
char taken[MAXP];

int fileexists(char *s) {
  FILE *f=fopen(s,"r");
  if(f) {
    fclose(f);
    return 1;
  }
  return 0;
}

int supported(char *s) {
  if(!strcmp(s,"nur")) return 1;
  if(!strcmp(s,"aka")) return 1;
  if(!strcmp(s,"hey")) return 1;
  if(!strcmp(s,"sli")) return 1;
  if(!strcmp(s,"hit")) return 1;
  if(!strcmp(s,"mas")) return 1;
  if(!strcmp(s,"shi")) return 1;
  if(!strcmp(s,"has")) return 1;
  if(!strcmp(s,"num")) return 1;
  if(!strcmp(s,"yaj")) return 1;
  if(!strcmp(s,"rip")) return 1;
  return 0;
}

char hex(char s) {
  if(s>='0' && s<='9') return s-48;
  if(s>='A' && s<='F') return s-'A'+10;
  if(s>='a' && s<='f') return s-'a'+10;
  return 0;
}

char grabchar(char *p) {
  if(*p!='%') return *p;
  else return hex(*(p+1))*16+hex(*(p+2));
}

#define EAT if(*p=='%') p+=3; else p++;
#define GRAB k=0;while(*p!='+' && *p!='%') t[k++]=*(p++); t[k]=0;

char convtall(int v) {
  if(v<10) return v+48;
  else if(v<36) return v-10+'a';
  else if(v<62) return v-36+'A';
  else {
    printf("too high number (%d)! patch manually\n",v);
    return '?';
  }
}

char convtall0(int v) {
  if(v==0) return '.';
  else if(v<10) return v+48;
  else if(v<36) return v-10+'a';
  else if(v<62) return v-36+'A';
  else {
    printf("too high number (%d)! patch manually\n",v);
    return '?';
  }
}

char *nurikabe(char *t) {
  static char s[1000];
  int v=strtol(t,NULL,10);
  if(v==0) strcpy(s,".");
  else if(v<10) sprintf(s,"%d",v);
  else if(v<36) sprintf(s,"%c",v-10+'a');
  else if(v<62) sprintf(s,"%c",v-36+'A');
  else sprintf(s,"{%d}",v);
  return s;
}

const char *finddata="dataQuestion=";
void parsenurikabe(FILE *f,int x,int y) {
  char *p=strstr(buffer,finddata)+strlen(finddata),t[100];
  int i,j,k;
  for(j=0;j<y;j++) {
    for(i=0;i<x;i++) {
      GRAB EAT
      fprintf(f,"%s",nurikabe(t));
    }
    fprintf(f,"\n");
  }
}

char akari(char c) {
  if(c=='X') return '#';
  if(c=='-') return '.';
  if(c>='0' && c<='4') return c;
  printf("ERROR %c\n",c);
  return 0;
}

void parseakari(FILE *f,int x,int y) {
  char *p=strstr(buffer,finddata)+strlen(finddata);
  char c;
  int i,j;
  for(j=0;j<y;j++) {
    for(i=0;i<x;i++) {
      c=grabchar(p); EAT EAT
      fprintf(f,"%c",akari(c));
    }
    fprintf(f,"\n");
  }
}

void parseheyawake(FILE *f,int x,int y) {
  static char g[512][512];
  char *p=strstr(buffer,finddata)+strlen(finddata);
  char t[100];
  int i,j,k;
  int v[50],n;
  for(i=0;i<=x*2;i++) for(j=0;j<=y*2;j++) g[i][j]='.';
  while(1) {
    if(*p=='&') break;
    n=0;
    while(*p!='%') {
      GRAB
      v[n]=strtol(t,NULL,10);
      if(n<4) v[n]*=2;
      n++;
      if(*p=='+') p++;
    }
    p+=3;
    g[v[0]][v[1]]='+';
    g[v[0]+v[2]][v[1]]='+';
    g[v[0]][v[1]+v[3]]='+';
    g[v[0]+v[2]][v[1]+v[3]]='+';
    /*  horizontal */
    for(i=v[0]+1;i<v[0]+v[2];i++) {
      if(g[i][v[1]]!='+') g[i][v[1]]='-';
      if(g[i][v[1]+v[3]]!='+') g[i][v[1]+v[3]]='-';
    }
    /*  vertical */
    for(i=v[1]+1;i<v[1]+v[3];i++) {
      if(g[v[0]][i]!='+') g[v[0]][i]='|';
      if(g[v[0]+v[2]][i]!='+') g[v[0]+v[2]][i]='|';
    }
    if(n>4) g[v[0]+1][v[1]+1]=convtall(v[4]);
  }
  for(j=0;j<=y*2;j++) {
    for(i=0;i<=x*2;i++) fprintf(f,"%c",g[i][j]);
    fprintf(f,"\n");
  }
}

void parseslitherlink(FILE *f,int x,int y) {
  char *p=strstr(buffer,finddata)+strlen(finddata);
  char c;
  int i,j;
  for(j=0;j<y;j++) {
    EAT
    for(i=0;i<x;i++) {
      c=grabchar(p); EAT EAT
      fprintf(f,"%c",akari(c));
    }
    fprintf(f,"\n");
  }
}

void parsehitori(FILE *f,int x,int y) {
  char *p=strstr(buffer,finddata)+strlen(finddata);
  char t[100];
  int i,j,k;
  for(j=0;j<y;j++) {
    EAT
    for(i=0;i<x;i++) {
      GRAB EAT
      fprintf(f,"%c",convtall(strtol(t,NULL,10)));
    }
    fprintf(f,"\n");
  }
}

char masyu(char c) {
  if(c=='0') return '.';
  if(c=='1') return 'o';
  if(c=='2') return '*';
  printf("ERROR masyu %c\n",c);
  return 0;
}

void parsemasyu(FILE *f,int x,int y) {
  char *p=strstr(buffer,finddata)+strlen(finddata);
  int i,j;
  for(j=0;j<y;j++) {
    for(i=0;i<x;i++) {
      fprintf(f,"%c",masyu(*p));
      if(i<x-1) p++;
      else p+=4;
    }
    fprintf(f,"\n");
  }
}

void parseshikaku(FILE *f,int x,int y) {
  char *p=strstr(buffer,finddata)+strlen(finddata);
  char t[100];
  int i,j,k;
  for(j=0;j<y;j++) {
    for(i=0;i<x;i++) {
      if(*p=='-') {
        fprintf(f,".");
        p++;
      } else {
        GRAB
        fprintf(f,"%c",convtall(strtol(t,NULL,10)));
      }
      EAT
    }
    fprintf(f,"\n");
  }
}

void parsehashiwokakero(FILE *f,int x,int y) {
  char *p=strstr(buffer,finddata)+strlen(finddata);
  int i,j;
  for(j=0;j<y;j++) {
    for(i=0;i<x;i++) {
      fprintf(f,"%c",convtall0(*p-48));
      if(i<x-1) p++;
      else p+=4;
    }
    fprintf(f,"\n");
  }
}

void parsenumberlink(FILE *f,int x,int y) {
  char *p=strstr(buffer,finddata)+strlen(finddata);
  char t[100];
  int i,j,k;
  for(j=0;j<y;j++) {
    EAT
    for(i=0;i<x;i++) {
      GRAB EAT
      fprintf(f,"%c",convtall0(strtol(t,NULL,10)));
    }
    fprintf(f,"\n");
  }
}

char yajilin(int v) {
  if(v<10) return v+48;
  else if(v<31) return v-10+'a';
  else if(v<35) return v-10+'a'+1;
  else if(v<61) return v-35+'A';
  else {
    printf("too high number (%d)! patch manually\n",v);
    return '?';
  }
}

char yajilindir(char c) {
  if(c=='L') return '<';
  if(c=='R') return '>';
  if(c=='U') return '^';
  if(c=='D') return 'v';
  printf("error yajilin dir %c\n",c);
  return 0;
}

void parseyajilin(FILE *f,int x,int y) {
  char *p=strstr(buffer,finddata)+strlen(finddata);
  static char g[512][512];
  int i,j,k,l,dx,dy,antall;
  for(j=0;j<y;j++) for(i=0;i<x;i++) {
    g[i][j]=*p;
    if(i<x-1) p++;
    else p+=4;
  }
  for(j=0;j<y;j++) {
    for(i=0;i<x;i++) {
      if(g[i][j]>='0' && g[i][j]<='9') fprintf(f,"..");
      else {
        dx=dy=0;
        if     (g[i][j]=='L') dx=-1;
        else if(g[i][j]=='R') dx=1;
        else if(g[i][j]=='U') dy=-1;
        else if(g[i][j]=='D') dy=1;
        k=i+dx;
        l=j+dy;
        antall=0;
        while(k>=0 && k<x && l>=0 && l<y) {
          if(g[k][l]=='0') antall++;
          k+=dx;
          l+=dy;
        }
        fprintf(f,"%c%c",yajilin(antall),yajilindir(g[i][j]));
      }
    }
    fprintf(f,"\n");
  }
}

char check(char h[512][512],int a,int b,int x,int y) {
  if(a<0 || b<0 || a>x*2 || b>y*2) return '.';
  if(h[a][b]>='0' && h[a][b]<='9') return '.';
  return h[a][b];
}

void parserippleeffect(FILE *f,int x,int y) {
  char *p=strstr(buffer,finddata)+strlen(finddata);
  static char g[512][512],h[512][512];
  int i,j;
  memset(h,'.',sizeof(h));
  for(j=0;j<y;j++) for(i=0;i<x;i++) {
    g[i][j]=*p;
    if(i<x-1) p++;
    else p+=4;
  }
  h[0][0]='+';
  h[2*x][0]='+';
  h[0][2*y]='+';
  h[2*x][2*y]='+';
  for(i=1;i<=x*2;i+=2) h[i][0]=h[i][y*2]='-';
  for(j=1;j<=y*2;j+=2) h[0][j]=h[x*2][j]='|';
  for(j=0;j<y;j++) {
    for(i=0;i<x;i++) {
      if(*p=='0') h[i*2+1][j*2+1]='.';
      else h[i*2+1][j*2+1]=*p;
      if(i<x-1) p++;
      else p+=4;
      if(i<x-1 && g[i][j]!=g[i+1][j]) h[i*2+2][j*2+1]='|';
      if(j<y-1 && g[i][j]!=g[i][j+1]) h[i*2+1][j*2+2]='-';
    }
  }
  for(i=0;i<=x*2;i+=2) for(j=0;j<=y*2;j+=2) {
    if(check(h,i-1,j,x,y)=='.' && check(h,i+1,j,x,y)=='.' && check(h,i,j+1,x,y)=='|' && check(h,i,j-1,x,y)=='|') h[i][j]='|';
    else if(check(h,i-1,j,x,y)=='-' && check(h,i+1,j,x,y)=='-' && check(h,i,j+1,x,y)=='.' && check(h,i,j-1,x,y)=='.') h[i][j]='-';
    else if(check(h,i-1,j,x,y)!='.' || check(h,i+1,j,x,y)!='.' || check(h,i,j+1,x,y)!='.' || check(h,i,j-1,x,y)!='.') h[i][j]='+';
  }
  for(j=0;j<y*2+1;j++) {
    for(i=0;i<x*2+1;i++) fprintf(f,"%c",h[i][j]);
    fprintf(f,"\n");
  }
}

void grabandsavepuzzle(char *s,char *firstline,char *p3,char *puzzle,char *difficulty) {
  const char *findsize="dataSize=";
  char *p;
  int x,y;
  FILE *f;
  p=strstr(buffer,findsize)+strlen(findsize);
  sscanf(p,"%d+%d",&x,&y);
  if(!(f=fopen(s,"w"))) {
    printf("error, couldn't save file %s.\n",s);
    return;
  }
  fprintf(f,"%s\n%s\n%s\n%d %d\n",firstline,puzzle,difficulty,x,y);
  if(!strcmp(p3,"nur")) parsenurikabe(f,x,y);
  if(!strcmp(p3,"aka")) parseakari(f,x,y);
  if(!strcmp(p3,"hey")) parseheyawake(f,x,y);
  if(!strcmp(p3,"sli")) parseslitherlink(f,x,y);
  if(!strcmp(p3,"hit")) parsehitori(f,x,y);
  if(!strcmp(p3,"mas")) parsemasyu(f,x,y);
  if(!strcmp(p3,"shi")) parseshikaku(f,x,y);
  if(!strcmp(p3,"has")) parsehashiwokakero(f,x,y);
  if(!strcmp(p3,"num")) parsenumberlink(f,x,y);
  if(!strcmp(p3,"yaj")) parseyajilin(f,x,y);
  if(!strcmp(p3,"rip")) parserippleeffect(f,x,y);
  fclose(f);
}

void processchamp() {
  const char *champmatch="<tr><td>Vol.",*findpuzzle="alt=\"Try ",*findpuzzle2="class=\"puzzle\">";
  const char *findauthor="</span></p></td><td>",*finddifficulty="<p><span>",*finddata="swf?loadUrl=";
	const char *endofrow="</tr>";
  char p3[4],puzzle[100],*q,*q2,*r,term,author[100],diff[100],s[100],url[100],t[1000];
	static char pp[BUF],*p=pp;
  int id,i,j;
	strcpy(pp,buffer);
  puts("start grabbing championship puzzles!");
  num=0;
  while((p=strstr(p,champmatch))) {
    p=q=p+strlen(champmatch);
		r=strstr(p,endofrow);
    id=strtol(q,NULL,10);
		q=strstr(p,findpuzzle);
		q2=strstr(p,findpuzzle2);
		if(q && q<r) {
			q+=strlen(findpuzzle);
			term=' ';
		} else {
      q=q2;
      q+=strlen(findpuzzle2);
      term='<';
		}
    for(i=0;i<3;i++) p3[i]=tolower(q[i]);
    for(i=0;q[i]!=term;i++) puzzle[i]=tolower(q[i]);
    puzzle[i]=p3[3]=0;
    if(!supported(p3)) continue;
    sprintf(s,"%sC%03d.txt",p3,id);
    if(fileexists(s)) continue;
    q=strstr(p,findauthor)+strlen(findauthor);
    for(i=0;q[i]!='<';i++) author[i]=q[i];
    author[i]=0;
    q=strstr(p,finddifficulty)+strlen(finddifficulty);
    for(i=0;q[i]!='<';i++) diff[i]=tolower(q[i]);
    diff[i]=0;
    printf("puzzle number %d: %s by %s, difficulty %s\n",id,puzzle,author,diff);
    q=strstr(p,finddata)+strlen(finddata);
    strcpy(url,"http://www.nikoli.com");
    for(i=0,j=strlen(url);q[i]!='&';i++,j++) url[j]=q[i];
    url[j]=0;
    loadwebpage(url,buffer);
    sprintf(t,"%% %s puzzle championship vol.%d nikoli by %s",puzzle,id,author);
    grabandsavepuzzle(s,t,p3,puzzle,diff);
    p=q;
    num++;
  }
}

void processsample() {
  const char *samplematch="<br />Sample problem",*findpuzzle="Sample problems of ";
  const char *findauthor="</p></td><td>",*finddifficulty="\"><p>",*finddata="swf?loadUrl=";
  char *p=buffer,*q,p3[4],puzzle[100],author[100],diff[100],s[100],url[100],t[1000];
  int id,i,j;
  p=strstr(p,findpuzzle);
  p+=strlen(findpuzzle);
  for(i=0;i<3;i++) p3[i]=tolower(p[i]);
  for(i=0;p[i]!=' ';i++) puzzle[i]=tolower(p[i]);
  puzzle[i]=p3[3]=0;
  if(!supported(p3)) {
    printf("no support for %s, yet\n",puzzle);
    return;
  }
  printf("start grabbing samples of %s!\n",puzzle);
  num=0;
  while((p=strstr(p,samplematch))) {
    q=p+strlen(samplematch);
    id=strtol(q,NULL,10);
    if(taken[id]) { p=q; continue; }
    sprintf(s,"%sS%02d.txt",p3,id);
    if(fileexists(s)) { p=q; continue; }
    q=strstr(p,findauthor)+strlen(findauthor);
    for(i=0;q[i]!='<';i++) author[i]=q[i];
    author[i]=0;
    q=strstr(p,finddifficulty)+strlen(finddifficulty);
    for(i=0;q[i]!='<';i++) diff[i]=tolower(q[i]);
    diff[i]=0;
    printf("sample puzzle %d by %s, difficulty %s\n",id,author,diff);
    q=strstr(p,finddata)+strlen(finddata);
    strcpy(url,"http://www.nikoli.com");
    for(i=0,j=strlen(url);q[i]!='&';i++,j++) url[j]=q[i];
    url[j]=0;
    if(url[j-4]=='h' && url[j-3]=='i' && url[j-2]=='s' && url[j-1]=='t') url[j-5]=0;
    loadwebpage(url,buffer);
    sprintf(t,"%% %s, nikoli sample problem %d by %s",puzzle,id,author);
    grabandsavepuzzle(s,t,p3,puzzle,diff);
    p=q;
    num++;
  }
}

/*  start of janko grab */

int supportedjanko(char *s) {
  if(!strcmp(s,"nur")) return 1;
  if(!strcmp(s,"hey")) return 1;
  if(!strcmp(s,"aka")) return 1;
  if(!strcmp(s,"pic")) return 1;
  if(!strcmp(s,"hit")) return 1;
  if(!strcmp(s,"sli")) return 1;
  if(!strcmp(s,"mas")) return 1;
  if(!strcmp(s,"has")) return 1;
  if(!strcmp(s,"yaj")) return 1;
  return 0;
}

void find3(char *s,char *t) {
  t[0]=t[1]=t[2]=t[3]=0;
  if(!strcmp(s,"akari")) strcpy(t,"aka");
  else if(!strcmp(s,"nurikabe")) strcpy(t,"nur");
  else if(!strcmp(s,"heyawake")) strcpy(t,"hey");
  else if(!strcmp(s,"hitori")) strcpy(t,"hit");
  else if(!strcmp(s,"masyu")) strcpy(t,"mas");
  else if(!strcmp(s,"slitherlink")) strcpy(t,"sli");
  else if(!strcmp(s,"hashiwokakero")) strcpy(t,"has");
  else if(!strcmp(s,"nonogramme")) {
    strcpy(t,"pic");
    strcpy(s,"picross");
  } else if(!strcmp(s,"yajilin")) strcpy(t,"yaj");
  else printf("[%s] not found\n",s);
}

void sleepms(int ms) {
  int tid=clock();
  while(clock()-tid<ms);
}

int parsenum(char *s) {
  int val=0;
  while(isdigit(*s)) val=val*10+(*(s++))-48;
  return val;
}

char *afternum(char *s) {
  while(isdigit(*s)) s++;
  return s;
}

char *token(char *p) {
  static char tok[256];
  int i=0;
  while(*p>' ' && *p<127 && *p!='"' && *p!='<' && *p!='>') tok[i++]=*(p++);
  tok[i]=0;
  return tok;
}

char *aftertoken(char *s) {
  while(isalnum(*s) || *s=='+') s++;
  return s;
}

char *numcode(int val) {
  static char s[1024];
  s[1]=0;
  if(val==0) s[0]='.';
  else if(val<10) s[0]=val+48;
  else if(val<36) s[0]=val-10+'a';
  else if(val<62) s[0]=val-36+'A';
  else sprintf(s,"{%d}",val);
  return s;
}

char *yajcode(int val) {
  static char s[1024];
  s[1]=0;
  if(val<10) s[0]=val+48;
  else if(val<31) s[0]=val-10+'a';
  else if(val<35) s[0]=val-10+'b';
  else if(val<61) s[0]=val-35+'A';
  else sprintf(s,"{%d}",val);
  return s;
}

void parsejankonur(FILE *f,char *p3,int x,int y) {
  char find[256],*p=buffer;
  int i,j;
  for(i=1;i<=y;i++) {
    sprintf(find,"name=\"p%d\" value=\"",i);
    p=strstr(p,find);
    if(!p) { printf("error parsing nurikabe %s\n",find); exit(1); }
    p+=strlen(find);
    for(j=0;j<x;j++) {
      if(*p=='-') fprintf(f,"."),p+=2;
      else {
        fprintf(f,"%s",numcode(parsenum(p)));
        p=afternum(p)+1;
      }
    }
    fprintf(f,"\n");
  }
}

void parsejankoaka(FILE *f,char *p3,int x,int y) {
  char find[256],*p=buffer;
  int i,j;
  for(i=1;i<=y;i++) {
    sprintf(find,"name=\"p%d\" value=\"",i);
    p=strstr(p,find);
    if(!p) { printf("error parsing akari %s\n",find); exit(1); }
    p+=strlen(find);
    for(j=0;j<x;j++) {
      if(*p=='-') fprintf(f,"."),p+=2;
      else if(*p=='x') fprintf(f,"#"),p+=2;
      else {
        fprintf(f,"%c",*p);
        p+=2;
      }
    }
    fprintf(f,"\n");
  }
}

void parsejankosli(FILE *f,char *p3,int x,int y) {
  char find[256],*p=buffer;
  int i,j;
  for(i=1;i<=y;i++) {
    sprintf(find,"name=\"p%d\" value=\"",i);
    p=strstr(p,find);
    if(!p) { printf("error parsing slitherlink %s\n",find); exit(1); }
    p+=strlen(find);
    for(j=0;j<x;j++) {
      if(*p=='-') fprintf(f,"."),p+=2;
      else {
        fprintf(f,"%c",*p);
        p+=2;
      }
    }
    fprintf(f,"\n");
  }
}

void parsejankomas(FILE *f,char *p3,int x,int y) {
  char find[256],*p=buffer;
  int i,j;
  for(i=1;i<=y;i++) {
    sprintf(find,"name=\"p%d\" value=\"",i);
    p=strstr(p,find);
    if(!p) { printf("error parsing masyu %s\n",find); exit(1); }
    p+=strlen(find);
    for(j=0;j<x;j++) {
      if(*p=='-') fprintf(f,"."),p+=2;
      else if(*p=='w') fprintf(f,"o"),p+=2;
      else if(*p=='b') fprintf(f,"*"),p+=2;
    }
    fprintf(f,"\n");
  }
}

void parsejankohit(FILE *f,char *p3,int x,int y) {
  char find[256],*p=buffer;
  int i,j;
  for(i=1;i<=y;i++) {
    sprintf(find,"name=\"p%d\" value=\"",i);
    p=strstr(p,find);
    if(!p) { printf("error parsing hitori %s\n",find); exit(1); }
    p+=strlen(find);
    for(j=0;j<x;j++) {
      fprintf(f,"%s",numcode(parsenum(p)));
      p=afternum(p)+1;
    }
    fprintf(f,"\n");
  }
}

void parsejankohas(FILE *f,char *p3,int x,int y) {
  char find[256],*p=buffer;
  int i,j;
  for(i=1;i<=y;i++) {
    sprintf(find,"name=\"p%d\" value=\"",i);
    p=strstr(p,find);
    if(!p) { printf("error parsing hashiwokakero %s\n",find); exit(1); }
    p+=strlen(find);
    for(j=0;j<x;j++) {
      if(*p=='-') fprintf(f,"."),p+=2;
      else fprintf(f,"%c",*p),p+=2;
    }
    fprintf(f,"\n");
  }
}

void parsejankopic(FILE *f,char *p3,int x,int y) {
  static char g[256][256];
  char find[256],*p=buffer;
  int i,j,first,cur;
  for(i=0;i<y;i++) {
    sprintf(find,"name=\"s%d\" value=\"",i+1);
    p=strstr(p,find);
    if(!p) { printf("error parsing picross %s\n",find); exit(1); }
    p+=strlen(find);
    for(j=0;j<x;j++) {
      g[j][i]=*p;
      p+=2;
    }
  }
  /*  vertical clues */
  for(i=0;i<y;i++) {
    first=1;
    for(cur=j=0;j<=x;j++) if(j==x || g[j][i]=='-') {
      if(cur) {
        if(first) first=0;
        else fprintf(f,",");
        fprintf(f,"%d",cur);
        cur=0;
      }
    } else cur++;
    if(first) fprintf(f,"0");
    fprintf(f,"\n");
  }
  /*  horizontal clues */
  for(i=0;i<x;i++) {
    first=1;
    for(cur=j=0;j<=y;j++) if(j==y || g[i][j]=='-') {
      if(cur) {
        if(first) first=0;
        else fprintf(f,",");
        fprintf(f,"%d",cur);
        cur=0;
      }
    } else cur++;
    if(first) fprintf(f,"0");
    fprintf(f,"\n");
  }
}

void parsejankoyaj(FILE *f,char *p3,int x,int y) {
  char find[256],*p=buffer,dir=' ';
  int i,j,v;
  for(i=1;i<=y;i++) {
    sprintf(find,"name=\"p%d\" value=\"",i);
    p=strstr(p,find);
    if(!p) { printf("error parsing masyu %s\n",find); exit(1); }
    p+=strlen(find);
    for(j=0;j<x;j++) {
      if(*p=='-') fprintf(f,".."),p+=2;
			else if(isdigit(*p)) {
				v=0;
				while(isdigit(*p)) v=v*10+*p-48,p++;
				if(*p=='n') dir='^';
				else if(*p=='s') dir='v';
				else if(*p=='w') dir='<';
				else if(*p=='e') dir='>';
				p+=2;
				fprintf(f,"%s%c",yajcode(v),dir);
			} else {
				p+=2;
				fprintf(f,"xx");
			}
    }
    fprintf(f,"\n");
  }
}

char *heycode(int val) {
  static char s[1024];
  s[1]=0;
  if(val<10) s[0]=val+48;
  else if(val<36) s[0]=val-10+'a';
  else if(val<62) s[0]=val-36+'A';
  else sprintf(s,"{%d}",val);
  return s;
}

void parsejankohey(FILE *f,char *p3,int x,int y) {
  char find[256],*p=buffer,*res;
  static char g[256][256];
  static char reg[256][256][16];
  char build[256];
  int i,j,k;
  for(i=0;i<=x*2;i++) for(j=0;j<=y*2;j++) g[j][i]='.';
  for(i=0;i<=y*2;i++) g[i][x*2+1]=0;
  for(i=0;i<=y*2;i++) g[i][0]=g[i][x*2]='|';
  for(i=0;i<=x*2;i++) g[0][i]=g[y*2][i]='-';
  g[0][0]=g[0][x*2]=g[y*2][0]=g[y*2][x*2]='+';
  for(j=0;j<y;j++) {
    sprintf(find,"name=\"a%d\" value=\"",j+1);
    p=strstr(p,find);
    if(!p) { printf("error parsing heyawake %s\n",find); exit(1); }
    p+=strlen(find);
    for(i=0;i<x;i++) {
      k=0;
      while(*p!=' ' && *p!='"') build[k++]=*(p++);
      build[k]=0;
      strcpy(reg[j][i],build);
      while(*p==' ') p++;
    }
  }
  /*  generate grid lines */
  for(j=0;j<y;j++) for(i=0;i<x;i++) {
    if(i<x-1 && strcmp(reg[j][i],reg[j][i+1])) g[j*2+1][i*2+2]='|';
    if(j<y-1 && strcmp(reg[j][i],reg[j+1][i])) g[j*2+2][i*2+1]='-';
  }
  /*  border */
  for(i=1;i<x*2;i++) {
    if(g[1][i]=='|') g[0][i]='+';
    if(g[y*2-1][i]=='|') g[y*2][i]='+';
  }
  for(i=1;i<y*2;i++) {
    if(g[i][1]=='-') g[i][0]='+';
    if(g[i][x*2-1]=='-') g[i][x*2]='+';
  }
  /*  fill in holes */
  for(i=1;i<x*2;i++) for(j=1;j<y*2;j++) if((!(i&1) || !(j&1)) && g[j][i]=='.') {
    if(g[j-1][i]=='|' && g[j+1][i]=='|' && g[j][i-1]=='.' && g[j][i+1]=='.') g[j][i]='|';
    else if(g[j-1][i]=='.' && g[j+1][i]=='.' && g[j][i-1]=='-' && g[j][i+1]=='-') g[j][i]='-';
    else if(g[j-1][i]=='.' && g[j+1][i]=='.' && g[j][i-1]=='.' && g[j][i+1]=='.') g[j][i]='.';
    else if(!(i&1) && !(j&1)) {
      if((g[j-1][i]!='.' || g[j+1][i]!='.') || (g[j][i-1]!='.' || g[j][i+1]!='.')) g[j][i]='+';
    }
  }
  p=buffer;
  for(j=0;j<y;j++) {
    sprintf(find,"name=\"p%d\" value=\"",j+1);
    p=strstr(p,find);
    if(!p) { printf("error parsing heyawake %s\n",find); exit(1); }
    p+=strlen(find);
    for(i=0;i<x;i++) {
      if(*p=='-') p+=2;
      else {
        res=heycode(parsenum(p));
        g[j*2+1][i*2+1]=res[0];
        if(res[0]=='{') printf("warning, edit in %s later\n",res);
        p=afternum(p)+1;
      } 
    }
  }
  for(i=0;i<=y*2;i++) fprintf(f,"%s\n",g[i]);
}

void parsejankopuzzle(FILE *f,char *p3,int x,int y) {
  if(!strcmp(p3,"nur")) parsejankonur(f,p3,x,y);
  else if(!strcmp(p3,"hey")) parsejankohey(f,p3,x,y);
  else if(!strcmp(p3,"aka")) parsejankoaka(f,p3,x,y);
  else if(!strcmp(p3,"hit")) parsejankohit(f,p3,x,y);
  else if(!strcmp(p3,"pic")) parsejankopic(f,p3,x,y);
  else if(!strcmp(p3,"sli")) parsejankosli(f,p3,x,y);
  else if(!strcmp(p3,"mas")) parsejankomas(f,p3,x,y);
  else if(!strcmp(p3,"has")) parsejankohas(f,p3,x,y);
  else if(!strcmp(p3,"yaj")) parsejankoyaj(f,p3,x,y);
  else return;
  num++;
}

void grabjanko(char *theurl,char *p3,int tot,char *puzzle) {
  char filename[32],paste[64],author[256],*p,diffstr[128];
  const char *findauthor="author\" value=\"";
  const char *findxsize="cols\" value=\"";
  const char *findysize="rows\" value=\"";
  const char *findsize="size\" value=\"";
  static char url[STR];
  int grabnum,i,x,y;
  FILE *f;
  for(grabnum=1;grabnum<=tot;grabnum++) {
    sprintf(filename,"%sR%04d.txt",p3,grabnum);
    if(fileexists(filename)) continue;
    strcpy(url,theurl);
    i=strlen(url);
    sprintf(paste,"%03d.a.htm",grabnum);
    strcat(url,paste);
    loadwebpage(url,buffer);
    if(!strstr(buffer,"<applet code=\"")) { printf("error loading %s %d [%s]\n",p3,grabnum,url); continue; }
    f=fopen(filename,"w");
    if(!f) continue;
    /*  find author */
    p=strstr(buffer,findauthor);
    if(!p) strcpy(author,"{unknown}");
    else {
      p+=strlen(findauthor);
      for(i=0;p[i] && p[i]!='"';i++) author[i]=p[i];
      author[i]=0;
    }
    /*  find x,y size */
    p=strstr(buffer,findxsize);
    if(!p) {
      p=strstr(buffer,findsize);
      if(!p) { printf("error, couldn't find size\n"); fclose(f); remove(filename); continue; }
      p+=strlen(findsize);
      x=y=parsenum(p);
      goto foundsize;
    }
    p+=strlen(findxsize);
    x=parsenum(p);
    p=strstr(buffer,findysize);
    if(!p) { printf("error, couldn't find size\n"); fclose(f); remove(filename); continue; }
    p+=strlen(findysize);
    y=parsenum(p);
    if(x>100 || y>100) {
      printf("error, puzzle is too large (%d %d)\n",x,y);
      fclose(f);
      remove(filename);
      continue;
    }
  foundsize:
    fprintf(f,"%% %s %d by %s, %s\n",puzzle,grabnum,author,url);
    if(diff[grabnum]==-1) strcpy(diffstr,"unknown");
    else strcpy(diffstr,diffstrs[diff[grabnum]]);
    fprintf(f,"%s\n%s\n%d %d\n",puzzle,diffstr,x,y);
    parsejankopuzzle(f,p3,x,y);
    fclose(f);
    printf("grab puzzle %d by %s of difficulty %s\n",grabnum,author,diffstr);
    sleepms(2000);
  }
}

int parsedifficulty(char *s) {
  if(!strcmp(s,"leicht")) return 0;
  if(!strcmp(s,"1")) return 1;
  if(!strcmp(s,"2")) return 2;
  if(!strcmp(s,"3")) return 3;
  if(!strcmp(s,"4")) return 4;
  if(!strcmp(s,"5")) return 5;
  if(!strcmp(s,"6")) return 6;
  if(!strcmp(s,"7")) return 7;
  if(!strcmp(s,"8")) return 8;
  if(!strcmp(s,"9")) return 9;
  if(!strcmp(s,"schwer")) return 10;
  printf("unknown difficulty: %s\n",s);
  return -1;
}

void processjanko(char *u,char *v) {
  const char *findpuzzle="<title>",*findnum="<a href=\"",*index="index.htm";
  const char *finddifficult="<h3>Sortiert nach Schw",*findtag="<th>";
  const char *findlevelno=".a.htm\">";
  char puzzle[128],*p,p3[4],*q,*r;
  static char theurl[STR];
  int num=-1,i,val,j;
  int curdiff,tall;
  strcpy(theurl,u);
  /*  determine number of puzzles: find highest number such that there exits
      <a href="num.a.htm">  */
  for(i=0;buffer[i];i++) if(!strncmp(buffer+i,findnum,strlen(findnum))) {
    j=i+strlen(findnum);
    if(!isdigit(buffer[j])) continue;
    val=0;
    while(isdigit(buffer[j])) val=val*10+buffer[j++]-48;
    if(val>num) num=val;
  }
  /*  grab difficulties */
  for(i=1;i<=num;i++) diff[i]=-1;
  if(v==NULL) p=buffer;
  else p=buf2;
  p=strstr(p,finddifficult);
  if(!p) goto nodiff;
  while(1) {
    p=strstr(p,findtag);
    if(!p) goto nodiff;
    p+=strlen(findtag);
    curdiff=parsedifficulty(token(p));
    while(1) {
      q=strstr(p,findtag);
      r=strstr(p,findlevelno);
      if(!q || !r) goto nodiff;
      if(q<r) break;
      r+=strlen(findlevelno);
      tall=parsenum(r);
      diff[tall]=curdiff;
      p=r;
    }
  }
nodiff:
  p=strstr(buffer,findpuzzle);
  if(!p) { printf("error fetching puzzle name"); return;}
  p+=strlen(findpuzzle);
  for(i=0;isalnum(p[i]);i++) puzzle[i]=p[i];
  puzzle[i]=0;
  puzzle[0]=tolower(puzzle[0]);
  find3(puzzle,p3);
  printf("grab %d puzzles of type %s (%s)\n",num,puzzle,p3);
  if(!p3[0]) { printf("error, no p3-code for [%s]\n",puzzle); return; }
  if(!supportedjanko(p3)) { printf("error, no grabbing support for %s (%s)\n",puzzle,p3); return; }
  /*  remove index.htm */
  if(!strcmp(theurl+strlen(theurl)-strlen(index),index)) theurl[strlen(theurl)-strlen(index)]=0;
  grabjanko(theurl,p3,num,puzzle);
}

int main(int argc,char**argv) {
  puts("puzzle grabber v0.2\n");
  if(argc<2) {
    puts("usage: grab website");
    puts("or: grab website website2 (for janko.at, where website is the main");
    puts("page for puzzles, and website2 is the difficulty");
    exit(0);
  }
  loadwebpage(argv[1],buffer);
  strcpy(buf2,buffer);
  if(strstr(buffer,"und Denksport")) {
    if(argc>2) {
      loadwebpage(argv[2],buf2);
      processjanko(argv[1],argv[2]);
    } else processjanko(argv[1],NULL);
  } else if(strstr(buffer,"Sample problems")) processsample();
  else processchamp();
  printf("done! grabbed %d puzzles\n",num);
  return 0;
}
