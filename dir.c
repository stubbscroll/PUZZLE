/* small re-entrant dir library by me, which is a wrapper for os-specific
   routines */
#include "dir.h"
#include "puzzle.h"

#ifdef _WIN32
int dirwin(dir_t *h) {
	h->dir=(h->f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?1:0;
	h->s=h->f.cFileName;
	h->len=((ull)h->f.nFileSizeHigh<<32)+h->f.nFileSizeLow;
	h->nolen=0;
	return 1;
}
#else
int dirunix(dir_t *h) {
	h->f=readdir(h->d);
	if(!h->f) return 0;
	h->s=h->f->d_name;
#ifdef _DIRENT_HAVE_D_TYPE
	h->dir=h->f->d_type==DT_DIR;
#else
	h->dir=-1;
#endif
	h->nolen=1;
	return 1;
}
#endif

/* return 1 if ok, 0 if error */
/* warning, windows requires * at the end of the path, while linux does not! */
int findfirst(char *const s,dir_t *h) {
#ifdef _WIN32
	h->hfind=FindFirstFile(s,&h->f);
	if(INVALID_HANDLE_VALUE==h->hfind) return 0;
	return dirwin(h);
#else
	h->d=opendir(s);
	if(!h->d) return 0;
	return dirunix(h);
#endif
}

/* return 0 if no more files in directory */
int findnext(dir_t *h) {
#ifdef _WIN32
	if(!FindNextFile(h->hfind,&h->f)) return 0;
	return dirwin(h);
#else
	return dirunix(h);
#endif
}

void findclose(dir_t *h) {
#ifdef _WIN32
	FindClose(h->hfind);
#else
	closedir(h->d);
#endif
}
