#ifndef INCLUDED_MAIN_X11_H_
#define INCLUDED_MAIN_X11_H_

#include "h-basic.h"
#include "ui-term.h"

enum x11_col_comp {
	X11_COL_COMP_ALPHA = 0,
	X11_COL_COMP_RED = 1,
	X11_COL_COMP_GREEN = 2,
	X11_COL_COMP_BLUE = 3
};

void x11_map_colors(void);
bool x11_init_tileset(void);

extern struct x11_tileset tileset;



#endif /* INCLUDED_X11_TERM_H_ */
