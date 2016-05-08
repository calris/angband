/*
 * x11-util.c
 *
 *  Created on: 3 May 2016
 *      Author: gruss
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>

#include "h-basic.h"
#include "z-virt.h"
#include "z-util.h"
#include "z-form.h"

#include "x11-util.h"

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
 */
struct x11_display {
	Display *display;
	Screen *screen;
	Window root;
	Colormap colormap;
	bool custom_colormap;
	XVisualInfo *visual_list;
	Visual *visual;

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

	bool color;
};

static int x11_display_init_visual(void);

static struct x11_display x11_display;

Display *x11_display_get()
{
	return x11_display.display;
}

/**
 * Hack -- cursor color
 */
static struct x11_color *xor_cursor_col;

void x11_alloc_cursor_col(void)
{
	xor_cursor_col = mem_zalloc(sizeof(struct x11_color));
	x11_color_init(xor_cursor_col, x11_display.fg, x11_display.bg, XOR, 0);
}

void x11_free_cursor_col(void)
{
	x11_color_nuke(xor_cursor_col);
	mem_free(xor_cursor_col);
}

/**
 * Find the square a particular pixel is part of.
 */
void x11_pixel_to_square(struct x11_term_data *td, int *x, int *y)
{
	*x = (*x - td->win->ox) / td->tile_width;
	*y = (*y - td->win->oy) / td->tile_height;
}

/**
 * Draw the cursor as a rectangular outline
 */
int x11_draw_curs(struct x11_term_data *td, int x, int y)
{
	XDrawRectangle(x11_display.display,
				   td->win->handle,
				   xor_cursor_col->gc,
				   x * td->tile_width + td->win->ox,
				   y * td->tile_height + td->win->oy,
				   td->tile_width - 1,
				   td->tile_height - 1);

	return 0;
}

/**
 * Draw the double width cursor as a rectangular outline
 */
int x11_draw_bigcurs(struct x11_term_data *td, int x, int y)
{
	XDrawRectangle(x11_display.display,
				   td->win->handle,
				   xor_cursor_col->gc,
				   x * td->tile_width + td->win->ox,
				   y * td->tile_height + td->win->oy,
				   td->tile_width2 - 1,
				   td->tile_height - 1);

	return 0;
}

/**
 * Look up keyboard modifiers using the Xkb extension
 */
static unsigned int xkb_mask_modifier(XkbDescPtr xkb, const char *name)
{
	unsigned int mask = 0;

	if (strcmp(name, "Caps Lock") == 0) {
		return 2;
	}

	for (int i = 0; (!mask) && (i <= XkbNumVirtualMods); i++) {
		char *mod_str = XGetAtomName(xkb->dpy, xkb->names->vmods[i]);

		if (mod_str) {
			if (!strcmp(name, mod_str)) {
				if (!XkbVirtualModsToReal(xkb, 1 << i, &mask)) {
					return 0;
				}
			}

			XFree(mod_str);
		}
	}

	return mask;
}

/**
 * Init the current metadpy, with various initialization stuff.
 *
 * Inputs:
 *	display:  The Display* to use (if NULL, create it)
 *	name: The name of the Display (if NULL, the current)
 *
 * Notes:
 *	If 'name' is NULL, but 'display' is set, extract name from display
 *	If 'display' is NULL, then Create the named Display
 *	If 'name' is NULL, and so is 'display', use current Display
 *
 * Return -1 if no Display given, and none can be opened.
 */
int x11_display_init(const char *name)
{
	XkbDescPtr xkb;

	/* Attempt to open the display */
	x11_display.display = XOpenDisplay(name);

	/* Failure */
	if (!x11_display.display) {
			return -1;
	}

	/* Get the Screen and Virtual Root Window */
	x11_display.screen = DefaultScreenOfDisplay(x11_display.display);
	x11_display.root = RootWindowOfScreen(x11_display.screen);
	x11_display.depth = DisplayPlanes(x11_display.display,
									  DefaultScreen(x11_display.display));

	/* get the modifier key layout */
	x11_display.alt_mask = Mod1Mask;
	x11_display.super_mask = Mod4Mask;

	xkb = XkbGetKeyboard(x11_display.display,
						 XkbAllComponentsMask,
						 XkbUseCoreKbd);

	if (xkb) {
		x11_display.alt_mask = xkb_mask_modifier(xkb, "Alt");
		x11_display.super_mask = xkb_mask_modifier(xkb, "Super");

		XkbFreeKeyboard(xkb, 0, true);
	}

	if (x11_display_init_visual() != 0) {
		x11_display_nuke();

		return -1;
	}

	/* Extract the true name of the display */
	x11_display.name = DisplayString(x11_display.display);

	/* Extract the fd */
	x11_display.fd = ConnectionNumber(x11_display.display);

	/* Save the Size and Depth of the screen */
	x11_display.width = WidthOfScreen(x11_display.screen);
	x11_display.height = HeightOfScreen(x11_display.screen);

	/* Save the Standard Colors */
	x11_display.black = BlackPixelOfScreen(x11_display.screen);
	x11_display.white = WhitePixelOfScreen(x11_display.screen);

	/* Guess at the desired 'fg' and 'bg' pixell's */
	x11_display.bg = x11_display.black;
	x11_display.fg = x11_display.white;

	/* Calculate the Maximum allowed Pixel value.  */
	x11_display.zg = ((pixell)1 << x11_display.depth) - 1;

	/* Save various default Flag Settings */
	x11_display.color = ((x11_display.depth > 1) ? true : false);

	return 0;
}

/**
 * Nuke the current metadpy
 */
int x11_display_nuke(void)
{
	if (x11_display.display) {
		XCloseDisplay(x11_display.display);
		x11_display.display = NULL;
	}

	if (x11_display.visual_list) {
		XFree(x11_display.visual_list);
		x11_display.visual_list = NULL;
	}

	if (x11_display.custom_colormap) {
		XFreeColormap(x11_display.display, x11_display.colormap);
		x11_display.colormap = 0;
	}

	return 0;
}

/**
 * General Flush/ Sync/ Discard routine
 */
int x11_display_update(int flush, int sync, int discard)
{
	if (flush) {
		XFlush(x11_display.display);
	}

	if (sync) {
		XSync(x11_display.display, discard);
	}

	return 0;
}

/**
 * Make a simple beep
 */
int x11_display_do_beep(void)
{
	XBell(x11_display.display, 100);

	return 0;
}

bool x11_display_is_color(void)
{
	return x11_display.color;
}

bool x11_display_mask_control(XKeyEvent *ev)
{
	return (ev->state & ControlMask) ? true : false;
}

bool x11_display_mask_shift(XKeyEvent *ev)
{
	return (ev->state & ShiftMask) ? true : false;
}

bool x11_display_mask_alt(XKeyEvent *ev)
{
	return (ev->state & x11_display.alt_mask) ? true : false;
}

bool x11_display_mask_super(XKeyEvent *ev)
{
	return (ev->state & x11_display.super_mask) ? true : false;
}

pixell x11_display_color_bg(void)
{
	return x11_display.bg;
}

pixell x11_display_color_fg(void)
{
	return x11_display.fg;
}

unsigned int x11_display_depth(void)
{
	return x11_display.depth;
}

unsigned long x11_visual_red_mask(void)
{
	return x11_display.visual->red_mask;
}

unsigned long x11_visual_green_mask(void)
{
	return x11_display.visual->green_mask;
}

unsigned long x11_visual_blue_mask(void)
{
	return x11_display.visual->blue_mask;
}

/**
 * Prepare a new 'infowin'.
 */
static int x11_window_prepare(struct x11_window *iwin)
{
	Window tmp_win;
	XWindowAttributes xwa;
	int x, y;
	unsigned int w, h, b, d;

	/* Check For Error XXX Extract some ACTUAL data from 'xid' */
	XGetGeometry(x11_display.display,
				 iwin->handle,
				 &tmp_win,
				 &x,
				 &y,
				 &w,
				 &h,
				 &b,
				 &d);

	/* Apply the above info */
	iwin->x = x;
	iwin->y = y;
	iwin->w = w;
	iwin->h = h;
	iwin->b = b;

	/* Check Error XXX Extract some more ACTUAL data */
	XGetWindowAttributes(x11_display.display, iwin->handle, &xwa);

	/* Apply the above info */
	iwin->mask = xwa.your_event_mask;

	return 0;
}

/**
 * Init an infowin by giving some data.
 *
 * Inputs:
 *	dad: The Window that should own this Window (if any)
 *	x,y: The position of this Window
 *	w,h: The size of this Window
 *	b,d: The border width and pixel depth
 */
int x11_window_init(struct x11_term_data *td,
					int x,
					int y,
					int w,
					int h,
					int b)
{
	Window xid;

	/* Create a top-window */
	td->win = mem_zalloc(sizeof(struct x11_window));

	/* Wipe it clean */
	memset(td->win, 0, sizeof(struct x11_window));

	xid = XCreateSimpleWindow(x11_display.display,
							  x11_display.root,
							  x,
							  y,
							  w,
							  h,
							  b,
							  x11_display.fg,
							  x11_display.bg);

	/* Start out selecting No events */
	XSelectInput(x11_display.display, xid, 0L);

	td->win->handle = xid;

	/* Attempt to Initialize the infowin */
	return x11_window_prepare(td->win);
}

/**
 * Nuke an X11 Window
 */
int x11_window_nuke(struct x11_term_data *td)
{
	if (td->win->gc) {
		XFreeGC(x11_display.display,td->win->gc);
		td->win->gc = 0;
	}

	if (td->win) {
		XDestroyWindow(x11_display.display, td->win->handle);
		mem_free(td->win);
	}

	return 0;
}

int x11_window_set_border(struct x11_term_data *td, s16b ox, s16b oy)
{
	td->win->ox = ox;
	td->win->oy = oy;

	return 0;
}

/**
 * Set the name (in the title bar) of an X11 Window
 */
int x11_window_set_name(struct x11_term_data *td, const char *name)
{
	Status st;
	XTextProperty tp;
	char buf[128];
	char *bp = buf;

	my_strcpy(buf, name, sizeof(buf));
	st = XStringListToTextProperty(&bp, 1, &tp);

	if (st) {
		XSetWMName(x11_display.display, td->win->handle, &tp);
	}

	XFree(tp.value);

	return 0;
}

/**
 * Modify the event mask of an X11 Window
 */
int x11_window_set_mask(struct x11_term_data *td, long mask)
{
	/* Save the new setting */
	td->win->mask = mask;

	/* Execute the Mapping */
	XSelectInput(x11_display.display, td->win->handle, td->win->mask);

	/* Success */
	return 0;
}

/**
 * Request that an X11 Window be mapped
 */
int x11_window_map(struct x11_term_data *td)
{
	XGCValues gcv;

	XMapWindow(x11_display.display, td->win->handle);

	td->win->gc = XCreateGC(x11_display.display, td->win->handle, 0, &gcv);

	return 0;
}

int x11_window_set_class_hint(struct x11_term_data *td, XClassHint *ch)
{
	XSetClassHint(x11_display.display, td->win->handle, ch);

	return 0;
}

int x11_window_set_size_hints(struct x11_term_data *td, XSizeHints *sh)
{
	XSetWMNormalHints(x11_display.display, td->win->handle, sh);

	return 0;
}

/**
 * Request that an X11 Window be raised
 */
int x11_window_raise(struct x11_term_data *td)
{
	XRaiseWindow(x11_display.display, td->win->handle);

	return 0;
}

/**
 * Request that an X11 Window be moved to a new location
 */
int x11_window_move(struct x11_term_data *td, int x, int y)
{
	XMoveWindow(x11_display.display, td->win->handle, x, y);

	return 0;
}

/**
 * Resize an X11 Window
 */
int x11_window_resize(struct x11_term_data *td, int w, int h)
{
	XResizeWindow(x11_display.display, td->win->handle, w, h);

	return 0;
}

/**
 * Visually clear an X11 Window
 */
int x11_window_wipe(struct x11_term_data *td)
{
	XClearWindow(x11_display.display, td->win->handle);

	return 0;
}

/**
 * Nuke an old 'infoclr'.
 */
int x11_color_nuke(struct x11_color *iclr)
{
	if (iclr->nuke) {
		XFreeGC(x11_display.display, iclr->gc);
	}

	return 0;
}

/**
 * Initialize an infoclr with some data
 *
 * Inputs:
 *	fg:   The pixell for the requested Foreground (see above)
 *	bg:   The pixell for the requested Background (see above)
 *	op:   The Opcode for the requested Operation (see above)
 *	stip: The stipple mode
 */
int x11_color_init(struct x11_color *iclr,
				   pixell fg,
				   pixell bg,
				   enum x11_function f,
				   int stip)
{
	GC gc;
	XGCValues gcv;
	unsigned long gc_mask;

	/* Check the 'pixells' for realism */
	if (bg > x11_display.zg) {
		return -1;
	}

	if (fg > x11_display.zg) {
		return -1;
	}

	/*** Create the requested 'GC' ***/

	/* Assign the proper GC function */
	gcv.function = f;

	/* Assign the proper GC background */
	gcv.background = bg;

	/* Assign the proper GC foreground */
	gcv.foreground = fg;

	/* Hack -- Handle XOR hacking bg and fg */
	if (f == XOR) {
		gcv.background = 0;
		gcv.foreground = (bg ^ fg);
	}

	/* Assign the proper GC Fill Style */
	gcv.fill_style = (stip ? FillStippled : FillSolid);

	/* Turn off 'Give exposure events for pixmap copying' */
	gcv.graphics_exposures = false;

	/* Set up the GC mask */
	gc_mask = (GCFunction |
			   GCBackground |
			   GCForeground |
			   GCFillStyle |
			   GCGraphicsExposures);

	/* Create the GC detailed above */
	gc = XCreateGC(x11_display.display, x11_display.root, gc_mask, &gcv);

	/* Wipe the iclr clean */
	memset(iclr, 0, sizeof(struct x11_color));

	/* Assign the GC */
	iclr->gc = gc;

	/* Nuke it when done */
	iclr->nuke = true;

	/* Assign the parms */
	iclr->fg = fg;
	iclr->bg = bg;

	/* Success */
	return 0;
}

/**
 * Change the 'fg' for an infoclr
 *
 * Inputs:
 *	fg:   The pixell for the requested Foreground (see above)
 */
int x11_color_change_fg(struct x11_color *iclr, pixell fg)
{
	/* Check the 'pixells' for realism */
	if (fg > x11_display.zg) {
		return -1;
	}

	XSetForeground(x11_display.display, iclr->gc, fg);

	/* Success */
	return 0;
}

bool x11_color_allocate(XColor *color)
{
	Colormap colormap = x11_display.colormap;

	return XAllocColor(x11_display.display, colormap, color) ? true : false;
}

/**
 * Prepare a new 'infofnt'
 */
static int x11_font_prepare(struct x11_font *ifnt, XFontSet fs)
{
	int font_count, i;
	char **names;
	XFontSetExtents *extents;
	XFontStruct **fonts;

	ifnt->fs = fs;
	extents = XExtentsOfFontSet(fs);
	font_count = XFontsOfFontSet(fs, &fonts, &names);
	ifnt->ascent = 0;

	for (i = 0; i < font_count; i++, fonts++) {
		if (ifnt->ascent < (*fonts)->ascent) {
			ifnt->ascent = (*fonts)->ascent;
		}
	}

	/* Extract default sizing info */
	ifnt->height = extents->max_logical_extent.height;
	ifnt->width = extents->max_logical_extent.width;

	return 0;
}

/**
 * Init an infofnt by its Name
 *
 * Inputs:
 *	name: The name of the requested Font
 */
int x11_font_init(struct x11_font *ifnt, const char *name)
{
	XFontSet fs;
	char **missing;
	int missing_count;

	if (!name) {
		return -1;
	}

	fs = XCreateFontSet(x11_display.display,
						name,
						&missing,
						&missing_count,
						NULL);

	if (!fs) {
		return -1;
	}

	if (missing_count) {
		XFreeStringList(missing);
	}

	memset(ifnt, 0, sizeof(struct x11_font));

	if (x11_font_prepare(ifnt, fs)) {
		XFreeFontSet(x11_display.display, fs);

		ifnt->nuke = false;

		return -1;
	}

	ifnt->name = string_make(name);
	ifnt->nuke = true;

	return 0;
}

/**
 * Nuke an old 'infofnt'.
 */
int x11_font_nuke(struct x11_font *ifnt)
{
	if (ifnt->name) {
		string_free(ifnt->name);
	}

	if (ifnt->nuke) {
		XFreeFontSet(x11_display.display, ifnt->fs);
	}

	return 0;
}

int x11_event_get(XEvent *xev, bool wait, void (*idle_update)(void))
{
	int i = 0;

	/* Do not wait unless requested */
	if (!wait && !XPending(x11_display.display)) {
		return 1;
	}

	/* Wait in 0.02s increments while updating animations every 0.2s */
	while (!XPending(x11_display.display)) {
		if (i == 0) {
			idle_update();
		}

		usleep(20000);
		i = (i + 1) % 10;
	}

	/* Load the Event */
	XNextEvent(x11_display.display, xev);

	return 0;
}

/**
 * Standard Text
 */
int x11_font_text_std(struct x11_term_data *td,
					  struct x11_color *fg_col,
					  struct x11_color *bg_col,
					  int x,
					  int y,
					  const wchar_t *str,
					  int len)
{
	int i;
	int w, h;

	/*** Do a brief info analysis ***/

	/* Do nothing if the string is null */
	if (!str || !*str) {
		return -1;
	}

	/* Get the length of the string */
	if (len < 0) {
		len = wcslen(str);
	}

	x = (x * td->tile_width) + td->win->ox;
	y = (y * td->tile_height) + td->win->oy;
	w = len * td->tile_width;
	h = td->tile_height;

	/* Fill the background */
	XFillRectangle(x11_display.display,
				   td->win->handle,
				   bg_col->gc,
				   x,
				   y,
				   w,
				   h);

	/*** Actually draw 'str' onto the infowin ***/
	y += td->font->ascent;

	/* Print each character of the string, monospaced*/
	for (i = 0; i < len; ++i) {
		XwcDrawImageString(x11_display.display,
						   td->win->handle,
						   td->font->fs,
						   fg_col->gc,
						   x + i * td->tile_width + td->font->off,
						   y,
						   str + i,
						   1);
	}

	return 0;
}

/**
 * Painting where text would be
 */
int x11_font_text_non(struct x11_term_data *td,
					  struct x11_color *iclr,
					  int x,
					  int y,
					  const wchar_t *str,
					  int len)
{
	int w, h;

	/*** Find the width ***/

	/* Negative length is a flag to count the characters in str */
	if (len < 0) {
		len = wcslen(str);
	}

	/* The total width will be 'len' chars * standard width */
	w = len * td->tile_width;

	/*** Find the X dimensions ***/

	/* Line up with x at left edge of column 'x' */
	x = x * td->tile_width + td->win->ox;

	/*** Find other dimensions ***/

	/* Simply do 'td->tile_height' (a single row) high */
	h = td->tile_height;

	/* Simply do "at top" in row 'y' */
	y = y * h + td->win->oy;

	/*** Actually 'paint' the area ***/

	/* Just do a Fill Rectangle */
	XFillRectangle(x11_display.display, td->win->handle, iclr->gc, x, y, w, h);

	/* Success */
	return 0;
}

static int x11_display_init_visual(void)
{
	bool need_colormap = false;
	XVisualInfo visual_info;

	if (x11_display.depth != 16 &&
		x11_display.depth != 24 &&
		x11_display.depth != 32) {
		int visuals_matched = 0;

		plog_fmt("default depth is %d:  checking other visuals",
				 x11_display.depth);

		/* 24-bit first */
		visual_info.screen = DefaultScreen(x11_display.display);
		visual_info.depth = 24;
		x11_display.visual_list = XGetVisualInfo(x11_display.display,
												 VisualScreenMask |
												 VisualDepthMask,
												 &visual_info,
												 &visuals_matched);

		if (visuals_matched == 0) {
			plog_fmt("screen depth %d not supported, and 24-bit visuals found",
					 x11_display.depth);
			return 2;
		}

		plog_fmt("XGetVisualInfo() returned %d 24-bit visuals",
				 visuals_matched);

		x11_display.visual = x11_display.visual_list[0].visual;
		x11_display.depth = x11_display.visual_list[0].depth;

		need_colormap = true;
	} else {
		XMatchVisualInfo(x11_display.display,
						 DefaultScreen(x11_display.display),
						 x11_display.depth,
						 TrueColor,
						 &visual_info);
		x11_display.visual = visual_info.visual;
	}

	if (x11_display.depth == 8 || need_colormap) {
		plog("Creating custom Colormap");

		x11_display.colormap = XCreateColormap(x11_display.display,
											   x11_display.root,
											   x11_display.visual,
											   AllocNone);

		if (!x11_display.colormap) {
			plog("XCreateColormap() failed");
			return 2;
		}

		x11_display.custom_colormap = true;
	} else {
		plog("Using default Colormap");
		x11_display.colormap = DefaultColormapOfScreen(x11_display.screen);
		x11_display.custom_colormap = false;
	}

	return 0;
}

XImage *x11_ximage_init(int format,
						int offset,
						char* data,
						unsigned int width,
						unsigned int height,
						int bitmap_pad,
						int bytes_per_line)
{
	if ((!x11_display.display) ||
		(!x11_display.visual) ||
		(!x11_display.depth)) {
		return NULL;
	}

	return XCreateImage(x11_display.display,
						x11_display.visual,
						x11_display.depth,
						format,
						offset,
						data,
						width,
						height,
						bitmap_pad,
						bytes_per_line);
}

bool x11_draw_tile(struct x11_term_data *td,
				   XImage *tiles,
				   int src_x,
				   int src_y,
				   int dest_x,
				   int dest_y,
				   unsigned int width,
				   unsigned int height)
{
	/* Do not draw anything that will go out of window bounds */
	if (((dest_x + width) > td->win->x) ||
		((dest_y + height) > td->win->y)) {
		return false;
	}

	XPutImage(x11_display.display,
			  td->win->handle,
			  td->win->gc,
			  tiles,
			  src_x,
			  src_y,
			  dest_x,
			  dest_y,
			  width,
			  height);

	return true;
}


