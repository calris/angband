/*
 * x11-util.h
 *
 *  Created on: 3 May 2016
 *      Author: gruss
 */

#ifndef CALRIS_ANGBAND_SRC_X11_UTIL_H_
#define CALRIS_ANGBAND_SRC_X11_UTIL_H_

#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>

/**
 * An X11 pixell specifier
 */
typedef unsigned long pixell;

/**
 * A structure summarizing a given Display.
 *
 *	- The Display itself
 *	- The default Screen for the display
 *	- The virtual root (usually just the root)
 *	- The default colormap (from a macro)
 *  - The Alt key modifier mask
 *  - The Super key modifier mask
 *
 *	- The "name" of the display
 *
 *	- The socket to listen to for events
 *
 *	- The width of the display screen (from a macro)
 *	- The height of the display screen (from a macro)
 *	- The bit depth of the display screen (from a macro)
 *
 *	- The black pixell (from a macro)
 *	- The white pixell (from a macro)
 *
 *	- The background pixell (default: black)
 *	- The foreground pixell (default: white)
 *	- The maximal pixell (Equals: ((2 ^ depth)-1), is usually ugly)
 *
 *	- Bit Flag: Force all colors to black and white (default: !color)
 *	- Bit Flag: Allow the use of color (default: depth > 1)
 *	- Bit Flag: We created 'dpy', and so should nuke it when done.
 */
struct x11_display {
	Display *dpy;
	Screen *screen;
	Window root;
	Colormap cmap;
	unsigned int alt_mask;
	unsigned int super_mask;

	char *name;

	int fd;

	unsigned int width;
	unsigned int height;
	unsigned int depth;

	pixell black;
	pixell white;

	pixell bg;
	pixell fg;
	pixell zg;

	unsigned int mono:1;
	unsigned int color:1;
	unsigned int nuke:1;
};

/**
 * A Structure summarizing Window Information.
 *
 * I assume that a window is at most 30000 pixels on a side.
 * I assume that the root windw is also at most 30000 square.
 *
 *	- The Window
 *	- The current Input Event Mask
 *
 *	- The location of the window
 *	- The saved (startup) location of the window
 *	- The width, height of the window
 *	- The border width of this window
 *
 *	- Byte: 1st Extra byte
 *
 *	- Bit Flag: This window is currently Mapped
 *	- Bit Flag: This window needs to be redrawn
 *	- Bit Flag: This window has been resized
 *
 *	- Bit Flag: We should nuke 'win' when done with it
 *
 *	- Bit Flag: 1st extra flag
 *	- Bit Flag: 2nd extra flag
 *	- Bit Flag: 3rd extra flag
 *	- Bit Flag: 4th extra flag
 */
struct x11_window {
	Window handle;
	long mask;

	int16_t ox, oy;

	int16_t x, y;
	int16_t x_save, y_save;
	int16_t w, h;
	uint16_t b;

	uint8_t byte1;

	unsigned int mapped:1;
	unsigned int redraw:1;
	unsigned int resize:1;

	unsigned int nuke:1;

	unsigned int flag1:1;
	unsigned int flag2:1;
	unsigned int flag3:1;
	unsigned int flag4:1;
};

/**
 * A Structure summarizing Operation+Color Information
 *
 *	- The actual GC corresponding to this info
 *
 *	- The Foreground pixell Value
 *	- The Background pixell Value
 *
 *	- Num (0-15): The operation code (As in Clear, Xor, etc)
 *	- Bit Flag: The GC is in stipple mode
 *	- Bit Flag: Destroy 'gc' at Nuke time.
 */
struct x11_colour {
	GC gc;

	pixell fg;
	pixell bg;

	unsigned int code:4;
	unsigned int stip:1;
	unsigned int nuke:1;
};

/**
 * A Structure to Hold Font Information
 *
 *	- The 'XFontStruct*' (yields the 'Font')
 *
 *	- The font name
 *
 *	- The default character width
 *	- The default character height
 *	- The default character ascent
 *
 *	- Byte: Pixel offset used during fake mono
 *
 *	- Flag: Force monospacing via 'wid'
 *	- Flag: Nuke info when done
 */
struct x11_font {
	XFontSet	fs;

	const char *name;

	int16_t wid;
	int16_t twid;
	int16_t hgt;
	int16_t asc;

	uint8_t off;

	unsigned int mono:1;
	unsigned int nuke:1;
};

/**
 * A structure for each "term"
 */
struct x11_term_data {
	struct x11_font *fnt;
	struct x11_window *win;

	int tile_wid;
	int tile_wid2; /* Tile-width with bigscreen */
	int tile_hgt;

	/* Pointers to allocated data, needed to clear up memory */
	XClassHint *classh;
	XSizeHints *sizeh;
};

enum x11_function {
	CPY = 3,
	XOR = 6
};

void x11_alloc_cursor_col(void);
void x11_free_cursor_col(void);

void x11_pixel_to_square(struct x11_term_data *td,
						 int * const x,
						 int * const y,
						 const int ox,
						 const int oy);

int x11_draw_curs(struct x11_term_data *td, int x, int y);
int x11_draw_bigcurs(struct x11_term_data *td, int x, int y);

int x11_display_init(Display *dpy, const char *name);
int x11_display_nuke(void);
int x11_display_update(int flush, int sync, int discard);
int x11_display_do_beep(void);

int x11_window_init(struct x11_window *iwin,
					int x,
					int y,
					int w,
					int h,
					int b,
					pixell fg,
					pixell bg);
int x11_window_nuke(struct x11_window *iwin);
int x11_window_set_border(struct x11_window *iwin, int16_t ox, int16_t oy);
int x11_window_set_name(struct x11_window *iwin, const char *name);
int x11_window_set_mask(struct x11_window *iwin, long mask);
int x11_window_map(struct x11_window *iwin);
int x11_window_set_class_hint(struct x11_window *iwin, XClassHint *ch);
int x11_window_set_size_hints(struct x11_window *iwin, XSizeHints *sh);
int x11_window_raise(struct x11_window *iwin);
int x11_window_move(struct x11_window *iwin, int x, int y);
int x11_window_resize(struct x11_window *iwin, int w, int h);
int x11_window_wipe(struct x11_window *iwin);

int x11_colour_init(struct x11_colour *iclr,
					pixell fg,
					pixell bg,
					enum x11_function f,
					int stip);
int x11_colour_nuke(struct x11_colour *iclr);
int x11_colour_change_fg(struct x11_colour *iclr, pixell fg);

int x11_font_init(struct x11_font *ifnt, const char *name);
int x11_font_nuke(struct x11_font *ifnt);

int x11_font_text_std(struct x11_term_data *td,
					 struct x11_colour *fg_col,
					 struct x11_colour *bg_col,
					 int x,
					 int y,
					 const wchar_t *str,
					 int len);

int x11_font_text_non(struct x11_term_data *td,
					 struct x11_colour *iclr,
					 int x,
					 int y,
					 const wchar_t *str,
					 int len);

extern struct x11_display *x11_display;


#ifndef IsModifierKey

/**
 * Stolen from one of the X11 header files
 */
#define IsModifierKey(keysym)		(((unsigned)(keysym) >= XK_Shift_L) && \
					 ((unsigned)(keysym) <= XK_Hyper_R))

#endif /* IsModifierKey */

/**
 * Checks if the keysym is a special key or a normal key
 * Assume that XK_MISCELLANY keysyms are special
 */
#define IsSpecialKey(keysym)	((unsigned)(keysym) >= 0xFF00)



#endif /* CALRIS_ANGBAND_SRC_X11_UTIL_H_ */
