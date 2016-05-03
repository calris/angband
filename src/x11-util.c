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

#include "x11-util.h"

#include "z-virt.h"
#include "z-util.h"

static struct metadpy metadpy_default;
struct metadpy *Metadpy = &metadpy_default;

/**
 * Hack -- cursor color
 */
static struct infoclr *xor;

void x11_alloc_cursor_col(void)
{
	xor = mem_zalloc(sizeof(struct infoclr));
	Infoclr_init_data(xor, Metadpy->fg, Metadpy->bg, XOR, 0);
}

void x11_free_cursor_col(void)
{
	Infoclr_nuke(xor);
	mem_free(xor);
}


int Infowin_set_border(struct infowin *iwin, int16_t ox, int16_t oy)
{
	iwin->ox = ox;
	iwin->oy = oy;

	return 0;
}

/**
 * Find the square a particular pixel is part of.
 */
void pixel_to_square(struct x11_term_data *td,
					 int * const x,
					 int * const y,
					 const int ox,
					 const int oy)
{
	*x = (ox - td->win->ox) / td->tile_wid;
	*y = (oy - td->win->oy) / td->tile_hgt;
}

/**
 * Draw the cursor as a rectangular outline
 */
int x11_term_curs(struct x11_term_data *td, int x, int y)
{
	XDrawRectangle(Metadpy->dpy,
				   td->win->handle,
				   xor->gc,
				   x * td->tile_wid + td->win->ox,
				   y * td->tile_hgt + td->win->oy,
				   td->tile_wid - 1,
				   td->tile_hgt - 1);

	return 0;
}

/**
 * Draw the double width cursor as a rectangular outline
 */
int x11_term_bigcurs(struct x11_term_data *td, int x, int y)
{
	XDrawRectangle(Metadpy->dpy,
				   td->win->handle,
				   xor->gc,
				   x * td->tile_wid + td->win->ox,
				   y * td->tile_hgt + td->win->oy,
				   td->tile_wid2 - 1,
				   td->tile_hgt - 1);

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
		char* mod_str = XGetAtomName(xkb->dpy, xkb->names->vmods[i]);

		if (mod_str) {
			if (!strcmp(name, mod_str)) {
				XkbVirtualModsToReal(xkb, 1 << i, &mask);
			}

			XFree(mod_str);
		}
	}
	return 0;
}

/**
 * Init the current metadpy, with various initialization stuff.
 *
 * Inputs:
 *	dpy:  The Display* to use (if NULL, create it)
 *	name: The name of the Display (if NULL, the current)
 *
 * Notes:
 *	If 'name' is NULL, but 'dpy' is set, extract name from dpy
 *	If 'dpy' is NULL, then Create the named Display
 *	If 'name' is NULL, and so is 'dpy', use current Display
 *
 * Return -1 if no Display given, and none can be opened.
 */
int Metadpy_init(Display *dpy, const char *name)
{
	struct metadpy *m = Metadpy;
	XkbDescPtr xkb;

	/* Attempt to create a display if none given, otherwise use the given one */
	if (!dpy) {
		/* Attempt to open the display */
		dpy = XOpenDisplay(name);

		/* Failure */
		if (!dpy) return (-1);

		/* We will have to nuke it when done */
		m->nuke = 1;
	} else {
		/* We will not have to nuke it when done */
		m->nuke = 0;
	}

	/* Save the Display itself */
	m->dpy = dpy;

	/* Get the Screen and Virtual Root Window */
	m->screen = DefaultScreenOfDisplay(dpy);
	m->root = RootWindowOfScreen(m->screen);

	/* get the modifier key layout */
	m->alt_mask = Mod1Mask;
	m->super_mask = Mod4Mask;
	xkb = XkbGetKeyboard(dpy, XkbAllComponentsMask, XkbUseCoreKbd);
	if (xkb != NULL) {
		m->alt_mask = xkb_mask_modifier( xkb, "Alt" );
		m->super_mask = xkb_mask_modifier( xkb, "Super" );
		XkbFreeKeyboard( xkb, 0, True );
	}

	/* Get the default colormap */
	m->cmap = DefaultColormapOfScreen(m->screen);

	/* Extract the true name of the display */
	m->name = DisplayString(dpy);

	/* Extract the fd */
	m->fd = ConnectionNumber(Metadpy->dpy);

	/* Save the Size and Depth of the screen */
	m->width = WidthOfScreen(m->screen);
	m->height = HeightOfScreen(m->screen);
	m->depth = DefaultDepthOfScreen(m->screen);

	/* Save the Standard Colors */
	m->black = BlackPixelOfScreen(m->screen);
	m->white = WhitePixelOfScreen(m->screen);

	/*** Make some clever Guesses ***/

	/* Guess at the desired 'fg' and 'bg' Pixell's */
	m->bg = m->black;
	m->fg = m->white;

	/* Calculate the Maximum allowed Pixel value.  */
	m->zg = ((Pixell)1 << m->depth) - 1;

	/* Save various default Flag Settings */
	m->color = ((m->depth > 1) ? 1 : 0);
	m->mono = ((m->color) ? 0 : 1);

	/* Return "success" */
	return (0);
}


/**
 * Nuke the current metadpy
 */
int Metadpy_nuke(void)
{
	struct metadpy *m = Metadpy;


	/* If required, Free the Display */
	if (m->nuke) {
		/* Close the Display */
		XCloseDisplay(m->dpy);

		/* Forget the Display */
		m->dpy = (Display*)(NULL);

		/* Do not nuke it again */
		m->nuke = 0;
	}

	/* Return Success */
	return (0);
}


/**
 * General Flush/ Sync/ Discard routine
 */
int Metadpy_update(int flush, int sync, int discard)
{
	/* Flush if desired */
	if (flush) XFlush(Metadpy->dpy);

	/* Sync if desired, using 'discard' */
	if (sync) XSync(Metadpy->dpy, discard);

	/* Success */
	return (0);
}


/**
 * Make a simple beep
 */
int Metadpy_do_beep(void)
{
	/* Make a simple beep */
	XBell(Metadpy->dpy, 100);

	return (0);
}



/**
 * Set the name (in the title bar) of Infowin
 */
int Infowin_set_name(struct infowin *iwin, const char *name)
{
	Status st;
	XTextProperty tp;
	char buf[128];
	char *bp = buf;

	my_strcpy(buf, name, sizeof(buf));
	st = XStringListToTextProperty(&bp, 1, &tp);

	if (st) {
		XSetWMName(Metadpy->dpy, iwin->handle, &tp);
	}

	XFree(tp.value);

	return 0;
}

/**
 * Nuke Infowin
 */
int Infowin_nuke(struct infowin *iwin)
{
	/* Nuke if requested */
	if (iwin->nuke) {
		/* Destory the old window */
		XDestroyWindow(Metadpy->dpy, iwin->handle);
	}

	/* Success */
	return (0);
}


/**
 * Prepare a new 'infowin'.
 */
static int Infowin_prepare(struct infowin *iwin, Window xid)
{
	Window tmp_win;
	XWindowAttributes xwa;
	int x, y;
	unsigned int w, h, b, d;

	/* Assign stuff */
	iwin->handle = xid;

	/* Check For Error XXX Extract some ACTUAL data from 'xid' */
	XGetGeometry(Metadpy->dpy, xid, &tmp_win, &x, &y, &w, &h, &b, &d);

	/* Apply the above info */
	iwin->x = x;
	iwin->y = y;
	iwin->x_save = x;
	iwin->y_save = y;
	iwin->w = w;
	iwin->h = h;
	iwin->b = b;

	/* Check Error XXX Extract some more ACTUAL data */
	XGetWindowAttributes(Metadpy->dpy, xid, &xwa);

	/* Apply the above info */
	iwin->mask = xwa.your_event_mask;
	iwin->mapped = ((xwa.map_state == IsUnmapped) ? 0 : 1);

	/* And assume that we are exposed */
	iwin->redraw = 1;

	/* Success */
	return (0);
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
int Infowin_init(struct infowin *iwin,
				 int x,
				 int y,
				 int w,
				 int h,
				 int b,
				 Pixell fg,
				 Pixell bg)
{
	Window xid;

	/* Wipe it clean */
	memset(iwin, 0, sizeof(struct infowin));

	xid = XCreateSimpleWindow(Metadpy->dpy,
							  Metadpy->root,
							  x,
							  y,
							  w,
							  h,
							  b,
							  fg,
							  bg);

	/* Start out selecting No events */
	XSelectInput(Metadpy->dpy, xid, 0L);

	/* Mark it as nukable */
	iwin->nuke = 1;

	/* Attempt to Initialize the infowin */
	return Infowin_prepare(iwin, xid);
}

/**
 * Modify the event mask of an Infowin
 */
int Infowin_set_mask(struct infowin *iwin, long mask)
{
	/* Save the new setting */
	iwin->mask = mask;

	/* Execute the Mapping */
	XSelectInput(Metadpy->dpy, iwin->handle, iwin->mask);

	/* Success */
	return 0;
}

int Infowin_set_class_hint(struct infowin *iwin, XClassHint *ch)
{
	XSetClassHint(Metadpy->dpy, iwin->handle, ch);

	return 0;
}

int Infowin_set_size_hints(struct infowin *iwin, XSizeHints *sh)
{
	XSetWMNormalHints(Metadpy->dpy, iwin->handle, sh);

	return 0;
}


/**
 * Request that Infowin be mapped
 */
int Infowin_map(struct infowin *iwin)
{
	XMapWindow(Metadpy->dpy, iwin->handle);

	return 0;
}

/**
 * Request that Infowin be raised
 */
int Infowin_raise(struct infowin *iwin)
{
	XRaiseWindow(Metadpy->dpy, iwin->handle);

	return 0;
}

/**
 * Request that Infowin be moved to a new location
 */
int Infowin_impell(struct infowin *iwin, int x, int y)
{
	XMoveWindow(Metadpy->dpy, iwin->handle, x, y);

	return 0;
}

/**
 * Resize an infowin
 */
int Infowin_resize(struct infowin *iwin, int w, int h)
{
	XResizeWindow(Metadpy->dpy, iwin->handle, w, h);

	return 0;
}

/**
 * Visually clear Infowin
 */
int Infowin_wipe(struct infowin *iwin)
{
	XClearWindow(Metadpy->dpy, iwin->handle);

	return 0;
}
/**
 * Nuke an old 'infoclr'.
 */
int Infoclr_nuke(struct infoclr *iclr)
{
	/* Deal with 'GC' */
	if (iclr->nuke) {
		/* Free the GC */
		XFreeGC(Metadpy->dpy, iclr->gc);
	}

	/* Success */
	return (0);
}

/**
 * Initialize an infoclr with some data
 *
 * Inputs:
 *	fg:   The Pixell for the requested Foreground (see above)
 *	bg:   The Pixell for the requested Background (see above)
 *	op:   The Opcode for the requested Operation (see above)
 *	stip: The stipple mode
 */
int Infoclr_init_data(struct infoclr *iclr,
					  Pixell fg,
					  Pixell bg,
					  enum x11_function f, int stip)
{
	GC gc;
	XGCValues gcv;
	unsigned long gc_mask;

	/* Check the 'Pixells' for realism */
	if (bg > Metadpy->zg) {
		return -1;
	}

	if (fg > Metadpy->zg) {
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
	gcv.graphics_exposures = False;

	/* Set up the GC mask */
	gc_mask = (GCFunction | GCBackground | GCForeground |
	           GCFillStyle | GCGraphicsExposures);

	/* Create the GC detailed above */
	gc = XCreateGC(Metadpy->dpy, Metadpy->root, gc_mask, &gcv);


	/*** Initialize ***/

	/* Wipe the iclr clean */
	(void)memset(iclr, 0, sizeof(struct infoclr));

	/* Assign the GC */
	iclr->gc = gc;

	/* Nuke it when done */
	iclr->nuke = 1;

	/* Assign the parms */
	iclr->fg = fg;
	iclr->bg = bg;
	iclr->code = (int)f;
	iclr->stip = stip ? 1 : 0;

	/* Success */
	return (0);
}



/**
 * Change the 'fg' for an infoclr
 *
 * Inputs:
 *	fg:   The Pixell for the requested Foreground (see above)
 */
int Infoclr_change_fg(struct infoclr *iclr, Pixell fg)
{
	/* Check the 'Pixells' for realism */
	if (fg > Metadpy->zg) {
		return -1;
	}

	XSetForeground(Metadpy->dpy, iclr->gc, fg);

	/* Success */
	return 0;
}



/**
 * Nuke an old 'infofnt'.
 */
int Infofnt_nuke(struct infofnt *ifnt)
{
	/* Deal with 'name' */
	if (ifnt->name) {
		/* Free the name */
		string_free((void *) ifnt->name);
	}

	/* Nuke info if needed */
	if (ifnt->nuke) {
		/* Free the font */
		XFreeFontSet(Metadpy->dpy, ifnt->fs);
	}

	/* Success */
	return (0);
}


/**
 * Prepare a new 'infofnt'
 */
static int Infofnt_prepare(struct infofnt *ifnt, XFontSet fs)
{
	int font_count, i;
	XFontSetExtents *extents;
	XFontStruct **fonts;
	char **names;

	/* Assign the struct */
	ifnt->fs = fs;
	extents = XExtentsOfFontSet(fs);

	font_count = XFontsOfFontSet(fs, &fonts, &names);
	ifnt->asc = 0;
	for (i = 0; i < font_count; i++, fonts++)
	   if (ifnt->asc < (*fonts)->ascent) ifnt->asc = (*fonts)->ascent;

	/* Extract default sizing info */
	ifnt->hgt = extents->max_logical_extent.height;
	ifnt->wid = extents->max_logical_extent.width;
	ifnt->twid = extents->max_logical_extent.width;

	/* Success */
	return (0);
}


/**
 * Init an infofnt by its Name
 *
 * Inputs:
 *	name: The name of the requested Font
 */
int Infofnt_init_data(struct infofnt *ifnt, const char *name)
{
	XFontSet fs;
	char **missing;
	int missing_count;

	/*** Load the info Fresh, using the name ***/

	/* If the name is not given, report an error */
	if (!name) {
		return -1;
	}

	fs = XCreateFontSet(Metadpy->dpy, name, &missing, &missing_count, NULL);

	/* The load failed, try to recover */
	if (!fs) {
		return -1;
	}

	if (missing_count) {
		XFreeStringList(missing);
	}

	/*** Init the font ***/

	/* Wipe the thing */
	memset(ifnt, 0, sizeof(struct infofnt));

	/* Attempt to prepare it */
	if (Infofnt_prepare(ifnt,fs)) {
		/* Free the font */
		XFreeFontSet(Metadpy->dpy, fs);

		/* Fail */
		return -1;
	}

	/* Save a copy of the font name */
	ifnt->name = string_make(name);

	/* Mark it as nukable */
	ifnt->nuke = 1;

	/* HACK - force all fonts to be printed character by character */
	ifnt->mono = 1;

	/* Success */
	return (0);
}


/**
 * Standard Text
 */
int Infofnt_text_std(struct x11_term_data *td,
					 struct infoclr *fg_col,
					 struct infoclr *bg_col,
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

	/*** Decide where to place the string, vertically ***/

	/* Ignore Vertical Justifications */
	y = (y * td->tile_hgt) + td->win->oy;


	/*** Decide where to place the string, horizontally ***/

	/* Line up with x at left edge of column 'x' */
	x = (x * td->tile_wid) + td->win->ox;

	/*** Erase the background ***/

	/* The total width will be 'len' chars * standard width */
	w = len * td->tile_wid;

	/* Simply do 'td->tile_hgt' (a single row) high */
	h = td->tile_hgt;

	/* Fill the background */
	XFillRectangle(Metadpy->dpy,
				   td->win->handle,
				   bg_col->gc,
				   x,
				   y,
				   w,
				   h);

	/*** Actually draw 'str' onto the infowin ***/
	y += td->fnt->asc;

	/*** Handle the fake mono we can enforce on fonts ***/

	/* Monotize the font */
	if (td->fnt->mono) {
		/* Do each character */
		for (i = 0; i < len; ++i) {
			/* Note that the Infoclr is set up to contain the Infofnt */
			XwcDrawImageString(Metadpy->dpy,
							   td->win->handle,
							   td->fnt->fs,
							   fg_col->gc,
							   x + i * td->tile_wid + td->fnt->off,
							   y,
							   str + i,
							   1);
		}
	} else {
		/* Note that the Infoclr is set up to contain the Infofnt */
		XwcDrawImageString(Metadpy->dpy,
						   td->win->handle,
						   td->fnt->fs,
						   fg_col->gc,
						   x,
						   y,
						   str,
						   len);
	}

	return 0;
}


/**
 * Painting where text would be
 */
int Infofnt_text_non(struct x11_term_data *td,
					 struct infoclr *iclr,
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
	w = len * td->tile_wid;

	/*** Find the X dimensions ***/

	/* Line up with x at left edge of column 'x' */
	x = x * td->tile_wid + td->win->ox;

	/*** Find other dimensions ***/

	/* Simply do 'td->tile_hgt' (a single row) high */
	h = td->tile_hgt;

	/* Simply do "at top" in row 'y' */
	y = y * h + td->win->oy;

	/*** Actually 'paint' the area ***/

	/* Just do a Fill Rectangle */
	XFillRectangle(Metadpy->dpy, td->win->handle, iclr->gc, x, y, w, h);

	/* Success */
	return 0;
}
