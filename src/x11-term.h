#ifndef INCLUDED_X11_TERM_H_
#define INCLUDED_X11_TERM_H_

#include "h-basic.h"
#include "ui-term.h"

void x11_term_install_hooks(struct term *t, bool graphical_term);

struct x11_term_config {
	int pos_x;
	int pos_y;

	char *font;

	int tile_width;
	int tile_height;

	int cols;
	int rows;

	int border_x;
	int border_y;
};

#endif /* INCLUDED_X11_TERM_H_ */
