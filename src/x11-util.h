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
typedef unsigned long Pixell;

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
 *	- The black Pixell (from a macro)
 *	- The white Pixell (from a macro)
 *
 *	- The background Pixell (default: black)
 *	- The foreground Pixell (default: white)
 *	- The maximal Pixell (Equals: ((2 ^ depth)-1), is usually ugly)
 *
 *	- Bit Flag: Force all colors to black and white (default: !color)
 *	- Bit Flag: Allow the use of color (default: depth > 1)
 *	- Bit Flag: We created 'dpy', and so should nuke it when done.
 */
struct metadpy {
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

	Pixell black;
	Pixell white;

	Pixell bg;
	Pixell fg;
	Pixell zg;

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
struct infowin {
	Window win;
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
 *	- The Foreground Pixell Value
 *	- The Background Pixell Value
 *
 *	- Num (0-15): The operation code (As in Clear, Xor, etc)
 *	- Bit Flag: The GC is in stipple mode
 *	- Bit Flag: Destroy 'gc' at Nuke time.
 */
struct infoclr {
	GC gc;

	Pixell fg;
	Pixell bg;

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
struct infofnt {
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
	struct infofnt *fnt;
	struct infowin *win;

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

void pixel_to_square(struct x11_term_data *td,
		     int * const x,
		     int * const y,
		     const int ox,
		     const int oy);

int x11_term_curs(struct x11_term_data *td, int x, int y);
int x11_term_bigcurs(struct x11_term_data *td, int x, int y);

int Metadpy_init(Display *dpy, const char *name);
int Metadpy_nuke(void);
int Metadpy_update(int flush, int sync, int discard);
int Metadpy_do_beep(void);

void Infowin_set(struct infowin *iwin);
void Infowin_set_border(int16_t ox, int16_t oy);
int Infowin_set_name(const char *name);
int Infowin_nuke(void);
int Infowin_init_top(int x, int y, int w, int h, int b, Pixell fg, Pixell bg);
int Infowin_set_mask(long mask);
int Infowin_map(void);
int Infowin_raise(void);
int Infowin_impell(int x, int y);
int Infowin_resize(int w, int h);
int Infowin_wipe(void);

int Infoclr_nuke(struct infoclr *iclr);
int Infoclr_init_data(struct infoclr *iclr,
					  Pixell fg,
					  Pixell bg,
					  enum x11_function f,
					  int stip);
int Infoclr_change_fg(struct infoclr *iclr, Pixell fg);

void Infofnt_set(struct infofnt *ifnt);
int Infofnt_nuke(void);
int Infofnt_init_data(const char *name);

int Infofnt_text_std(struct x11_term_data *td,
					 struct infoclr *fg_col,
					 struct infoclr *bg_col,
					 int x,
					 int y,
					 const wchar_t *str,
					 int len);

int Infofnt_text_non(struct x11_term_data *td,
					 struct infoclr *iclr,
					 int x,
					 int y,
					 const wchar_t *str,
					 int len);

extern struct metadpy *Metadpy;
extern struct infowin *Infowin;


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
