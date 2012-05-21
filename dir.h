#ifndef DIR_H
#define DIR_H

int findfirstfile(const char *);
int findnextfile(const char *);
void findclose();

typedef struct {
  char *filename;   /*  file name */
  int dir;          /*  1 if directory, 0 if file */
  unsigned int losize;  /*  file size low part */
  unsigned int hisize;  /*  file size high part */
  int y,m,d,hh,mm,ss;   /*  file date and time */
} dirfileinfo_t;

#endif
