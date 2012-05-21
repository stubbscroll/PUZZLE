/*  SDL font library by Ruben Spaans in 2011. Released under the GNU General
    Public Licence v3.

    Adds simple font and text functionality to SDL.
    - The font must be defined in a BMP file.
    - The font must contain no more than 224 characters, starting from ASCII
      code 32.
    - The two topmost pixels in the leftmost column define the font height:
      h=y2-y1-1
    - Each block of h+1 pixels (in height) of the image can contain
      characters.
    - Within a block, the topmost pixels determine the start and end x-
      coordinates for the characters. The last character within a block ends 
      with a top right pixel.
    - ASCII codes, starting at 32, are assigned in scanline order.
    - The image must have at most three colours: The dots, the background and
      a tertiary colour used in the fonts (optional). It doesn't matter which
      colour they are, but there must be consistency.
    - For reasons of efficiency and laziness, only output to 32-bit surfaces
      are supported!

    The caller is expected to lock any surfaces whenever necessary.
*/

#include <stdlib.h>
#include "SDL/SDL.h"
#include "sdlfont.h"
#define SDL_FONT_S 65536

/*  extra space between characters */
static int padw=1;
static int padh=1;

sdl_font *sdl_font_load(char *name) {
  sdl_font *font;
  SDL_Surface *surface;
  int bpp,i,j,pitch,x,y,atx,aty,atc;
  Uint8 fc,bc,*p;
  if(!(surface=SDL_LoadBMP(name))) return 0;
  p=surface->pixels;
  pitch=surface->pitch;
  x=surface->w;
  y=surface->h;
  bpp=surface->format->BytesPerPixel;
  /*  TODO convert to 8-bits */
  if(bpp==2) {
    SDL_FreeSurface(surface);
    return 0;
  } else if(bpp==3) {
    SDL_FreeSurface(surface);
    return 0;
  } else if(bpp==4) {
    SDL_FreeSurface(surface);
    return 0;
  } else if(bpp!=1) {
    /*  some unknown pixel format, throw error */
    SDL_FreeSurface(surface);
    return 0;
  }
  font=(sdl_font *)malloc(sizeof(sdl_font));
  font->font=surface;
  /*  normalize font bitmap colours */
  fc=p[0];
  bc=p[pitch];
  for(j=0;j<y;j++) for(i=0;i<x;i++) {
    if(p[i*bpp+j*pitch]==bc) p[i*bpp+j*pitch]=0;
    else if(p[i*bpp+j*pitch]==fc) p[i*bpp+j*pitch]=255;
    else p[i*bpp+j*pitch]=128;
  }
  /*  update font structure */
  i=1;
  while(!p[i*pitch]) i++;
  font->height=i-1;
  /*  scan to find (x,y) coordinates and width */
  atc=atx=aty=0;
  while(aty<y && p[aty*pitch]==255) {
    for(i=atx+1;i<x;i++) if(p[i*bpp+aty*pitch]==255) break;
    if(i==x) {
      atx=0;
      aty+=font->height+1;
      continue;
    }
    font->x[atc]=atx+1;
    font->y[atc]=aty+1;
    font->w[atc++]=i-atx-1;
    atx=i;
  }
  return font;
}

/*  free and destroy an existing font */
void sdl_font_free(sdl_font *font) {
  SDL_FreeSurface(font->font);
  free(font);
}

/*  TODO  desired functions:

    output string to 8-bit surface (opaque)
*/

/*  print text (transparent) */
/*  prints to 32-bit surface! */
void sdl_font_printf(SDL_Surface *surface,sdl_font *font,int x,int y,Uint32 col1,Uint32 col2,char *fmt,...) {
  static char t[SDL_FONT_S];
  int i,j,k,maxx=surface->w,maxy=surface->h,w,h=font->height,atx=x,c,xx,yy,fp=font->font->pitch;
  int pitch=surface->pitch>>2;
  int bpp=surface->format->BytesPerPixel>>2;
  Uint8 *p=font->font->pixels,v;
  Uint32 *q=surface->pixels;
  va_list argptr;
  va_start(argptr,fmt);
  vsprintf(t,fmt,argptr);
  va_end(argptr);
  for(k=0;t[k];k++) {
    c=(unsigned char)t[k]-32;
    if((unsigned char)t[k]<32) {
      if((unsigned char)t[k]==13) atx=x,y+=h+padh;
      continue;
    }
    w=font->w[c];
    xx=font->x[c];
    yy=font->y[c];
    for(j=0;j<h;j++) for(i=0;i<w;i++) if(i+xx>-1 && i+xx<maxx && j+yy>-1 && j+yy<maxy) {
      v=p[(i+xx)*bpp+(yy+j)*fp];
      if(v==255) q[(atx+i)*bpp+(y+j)*pitch]=col1;
      else if(v==128) q[(atx+i)*bpp+(y+j)*pitch]=col2;
    }
    atx+=w+padw;
  }
}

int sdl_font_width(sdl_font *font,char *fmt,...) {
  static char t[SDL_FONT_S];
  int w=-padw,i;
  va_list argptr;
  va_start(argptr,fmt);
  vsprintf(t,fmt,argptr);
  va_end(argptr);
  for(i=0;t[i];i++) if((unsigned char)t[i]>31) w+=padw+font->w[(unsigned char)t[i]-32];
  return w<0?0:w;
}

void changepadding(int x,int y) {
  padw=x; padh=y;
}
