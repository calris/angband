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

#include "h-basic.h"

/**
 * An X11 pixell specifier
 */
typedef unsigned long pixell;

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
	GC gc;
	long mask;

	s16b ox, oy;

	s16b x, y;
	s16b w, h;
	u16b b;
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
struct x11_color {
	GC gc;

	pixell fg;
	pixell bg;

	bool nuke;
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

	char *name;

	s16b width;
	s16b height;
	s16b ascent;

	byte off;

	bool nuke;
};

/**
 * A structure for each "term"
 */
struct x11_term_data {
	struct x11_font *font;
	struct x11_window *win;

	int tile_width;
	int tile_width2; /* Tile-width with bigscreen */
	int tile_height;

	XClassHint *classh;
	XSizeHints *sizeh;
};

struct x11_tileset {
	XImage *ximage;

	int overdraw;
	int overdrawmax;
	int alphablend;

	int tile_width;
	int tile_height;

	const char *name;
	const char *path;
};

enum x11_function {
	CPY = 3,
	XOR = 6
};

Display *x11_display_get();

void x11_alloc_cursor_col(void);
void x11_free_cursor_col(void);

void x11_pixel_to_square(struct x11_term_data *td, int *x, int *y);

int x11_draw_curs(struct x11_term_data *td, int x, int y);
int x11_draw_bigcurs(struct x11_term_data *td, int x, int y);

int x11_display_init(const char *name);
int x11_display_nuke(void);
int x11_display_update(int flush, int sync, int discard);
int x11_display_do_beep(void);
bool x11_display_is_color(void);
bool x11_display_mask_control(XKeyEvent *ev);
bool x11_display_mask_shift(XKeyEvent *ev);
bool x11_display_mask_alt(XKeyEvent *ev);
bool x11_display_mask_super(XKeyEvent *ev);
pixell x11_display_color_bg(void);
pixell x11_display_color_fg(void);
unsigned int x11_display_depth(void);

unsigned long x11_visual_red_mask(void);
unsigned long x11_visual_green_mask(void);
unsigned long x11_visual_blue_mask(void);

int x11_window_init(struct x11_term_data *td,
					int x,
					int y,
					int w,
					int h,
					int b);
int x11_window_nuke(struct x11_term_data *td);
int x11_window_set_border(struct x11_term_data *td, s16b ox, s16b oy);
int x11_window_set_name(struct x11_term_data *td, const char *name);
int x11_window_set_mask(struct x11_term_data *td, long mask);
int x11_window_map(struct x11_term_data *td);
int x11_window_set_class_hint(struct x11_term_data *td, XClassHint *ch);
int x11_window_set_size_hints(struct x11_term_data *td, XSizeHints *sh);
int x11_window_raise(struct x11_term_data *td);
int x11_window_move(struct x11_term_data *td, int x, int y);
int x11_window_resize(struct x11_term_data *td, int w, int h);
int x11_window_wipe(struct x11_term_data *td);

int x11_color_init(struct x11_color *iclr,
				   pixell fg,
				   pixell bg,
				   enum x11_function f,
				   int stip);
int x11_color_nuke(struct x11_color *iclr);
int x11_color_change_fg(struct x11_color *iclr, pixell fg);
bool x11_color_allocate(XColor *color);

int x11_font_init(struct x11_font *ifnt, const char *name);
int x11_font_nuke(struct x11_font *ifnt);

int x11_font_text_std(struct x11_term_data *td,
					  struct x11_color *fg_col,
					  struct x11_color *bg_col,
					  int x,
					  int y,
					  const wchar_t *str,
					  int len);

int x11_font_text_non(struct x11_term_data *td,
					  struct x11_color *iclr,
					  int x,
					  int y,
					  const wchar_t *str,
					  int len);

int x11_event_get(XEvent *xev, bool wait, void (*idle_update)(void));

XImage *x11_ximage_init(int format,
						int offset,
						char* data,
						unsigned int width,
						unsigned int height,
						int bitmap_pad,
						int bytes_per_line);

bool x11_draw_tile(struct x11_term_data *td,
				   XImage *tiles,
				   int src_x,
				   int src_y,
				   int dest_x,
				   int dest_y,
				   unsigned int width,
				   unsigned int height);
/**
 * Checks if the keysym is a special key or a normal key
 * Assume that XK_MISCELLANY keysyms are special
 */
#define IsSpecialKey(keysym)	((KeySym)(keysym) >= 0xFF00)

#endif /* CALRIS_ANGBAND_SRC_X11_UTIL_H_ */
