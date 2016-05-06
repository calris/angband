#ifndef INCLUDED_X11_PNG_H_
#define INCLUDED_X11_PNG_H_

#include <stdio.h>
#include <X11/Xlib.h>
#include <png.h>

typedef struct x11_png_image x11_png_image_t;

x11_png_image_t *x11_png_image_init(void);
void x11_png_image_nuke(x11_png_image_t *png_image);
bool x11_png_image_load(x11_png_image_t *png_image, const char *filename);
XImage *x11_png_create_ximage(x11_png_image_t *png_image);


#endif /* INCLUDED_X11_PNG_H_ */
