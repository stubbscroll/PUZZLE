#ifndef DIR_H
#define DIR_H

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <dirent.h>
#endif

typedef long long ll;
typedef unsigned long long ull;

typedef struct {
#ifdef _WIN32
	HANDLE hfind;
	WIN32_FIND_DATA f;
#else
	DIR *d;
	struct dirent *f;
#endif
	int dir;	/* 1: dir, 0: no dir, -1: not supported */
	ull len;
	int nolen;	/* 1 if no support for filelen */
	char *s;
} dir_t;

int findfirst(char *const,dir_t *);
int findnext(dir_t *);
void findclose(dir_t *);

#endif
