#ifndef SDLFONT_H
#define SDLFONT_H

#include <SDL/SDL.h>

/*  font structure for chars with ascii codes [32,255] */
typedef struct {
  int x[224],y[224];    /*  upper left coordinate for every char */
  int w[224];           /*  width for every char */
  int height;           /*  height of font */
  SDL_Surface *font;    /*  bitmap containing the font */
} sdl_font;

sdl_font *sdl_font_load(char *);
void sdl_font_free(sdl_font *);

void sdl_font_printf(SDL_Surface *,sdl_font *,int,int,Uint32,Uint32,char *,...);
int sdl_font_width(sdl_font *,char *,...);
void changepadding(int,int);

#endif /* SDLFONT_H */
