/*
 * x11-term.c
 *
 *  Created on: 8 May 2016
 *      Author: gruss
 */

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "h-basic.h"
#include "ui-term.h"
#include "ui-display.h"
#include "ui-prefs.h"
#include "grafmode.h"
#include "main-x11.h"
#include "x11-util.h"
#include "x11-term.h"

#define X11_TERM_DATA			((struct x11_term_data *)Term->data)
#define X11_TERM_0_DATA			((struct x11_term_data *)angband_term[0]->data)

static int map_keysym(KeySym ks, byte *mods)
{
	int ch = 0;

	switch (ks) {
		case XK_BackSpace: {
			ch = KC_BACKSPACE;
			break;
		}

		case XK_Tab: {
			ch = KC_TAB;
			break;
		}

		case XK_Return: {
			ch = KC_ENTER;
			break;
		}

		case XK_Escape: {
			ch = ESCAPE;
			break;
		}

		case XK_Delete: {
			ch = KC_DELETE;
			break;
		}

		case XK_Home: {
			ch = KC_HOME;
			break;
		}

		case XK_Left: {
			ch = ARROW_LEFT;
			break;
		}

		case XK_Up: {
			ch = ARROW_UP;
			break;
		}

		case XK_Right: {
			ch = ARROW_RIGHT;
			break;
		}

		case XK_Down: {
			ch = ARROW_DOWN;
			break;
		}

		case XK_Page_Up: {
			ch = KC_PGUP;
			break;
		}

		case XK_Page_Down: {
			ch = KC_PGDOWN;
			break;
		}

		case XK_End: {
			ch = KC_END;
			break;
		}

		case XK_Insert: {
			ch = KC_INSERT;
			break;
		}

		case XK_Pause: {
			ch = KC_PAUSE;
			break;
		}

		case XK_Break: {
			ch = KC_BREAK;
			break;
		}

		/* keypad */
		case XK_KP_0: {
			ch = '0';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_1: {
			ch = '1';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_2: {
			ch = '2';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_3: {
			ch = '3';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_4: {
			ch = '4';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_5: {
			ch = '5';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_6: {
			ch = '6';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_7: {
			ch = '7';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_8: {
			ch = '8';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_9: {
			ch = '9';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Decimal: {
			ch = '.';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Divide: {
			ch = '/';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Multiply: {
			ch = '*';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Subtract: {
			ch = '-';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Add: {
			ch = '+';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Enter: {
			ch = KC_ENTER;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Equal: {
			ch = '=';
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Delete: {
			ch = KC_DELETE;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Home: {
			ch = KC_HOME;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Left: {
			ch = ARROW_LEFT;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Up: {
			ch = ARROW_UP;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Right: {
			ch = ARROW_RIGHT;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Down: {
			ch = ARROW_DOWN;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Page_Up: {
			ch = KC_PGUP;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Page_Down: {
			ch = KC_PGDOWN;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_End: {
			ch = KC_END;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Insert: {
			ch = KC_INSERT;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_KP_Begin: {
			ch = KC_BEGIN;
			*mods |= KC_MOD_KEYPAD;
			break;
		}

		case XK_F1: {
			ch = KC_F1;
			break;
		}

		case XK_F2: {
			ch = KC_F2;
			break;
		}

		case XK_F3: {
			ch = KC_F3;
			break;
		}

		case XK_F4: {
			ch = KC_F4;
			break;
		}

		case XK_F5: {
			ch = KC_F5;
			break;
		}

		case XK_F6: {
			ch = KC_F6;
			break;
		}

		case XK_F7: {
			ch = KC_F7;
			break;
		}

		case XK_F8: {
			ch = KC_F8;
			break;
		}

		case XK_F9: {
			ch = KC_F9;
			break;
		}

		case XK_F10: {
			ch = KC_F10;
			break;
		}

		case XK_F11: {
			ch = KC_F11;
			break;
		}

		case XK_F12: {
			ch = KC_F12;
			break;
		}

		case XK_F13: {
			ch = KC_F13;
			break;
		}

		case XK_F14: {
			ch = KC_F14;
			break;
		}

		case XK_F15: {
			ch = KC_F15;
			break;
		}
	}

	return ch;
}

/**
 * Process a keypress event
 */
static void react_keypress(XKeyEvent *ev)
{
	int n, ch = 0;

	KeySym ks;

	char buf[128];

	/* Extract "modifier flags" */
	int mc = x11_display_mask_control(ev);
	int ms = x11_display_mask_shift(ev);
	int mo = x11_display_mask_alt(ev);
	int mx = x11_display_mask_super(ev);

	byte mods = (mo ? KC_MOD_ALT : 0) | (mx ? KC_MOD_META : 0);

	/* Check for "normal" keypresses */
	n = XLookupString(ev, buf, 125, &ks, NULL);
	buf[n] = '\0';

	/* Ignore modifier keys by themselves */
	if (IsModifierKey(ks)) {
		return;
	}

	/* Map the X Windows KeySym to it's Angband equivalent */
	ch = map_keysym(ks, &mods);

	if (ch) {
		if (mc) {
			mods |= KC_MOD_CONTROL;
		}

		if (ms) {
			mods |= KC_MOD_SHIFT;
		}

		Term_keypress(ch, mods);

		return;
	} else if (n && !IsSpecialKey(ks)) {
		keycode_t code = buf[0];

		if (mc && MODS_INCLUDE_CONTROL(code)) {
			mods |= KC_MOD_CONTROL;
		}

		if (ms && MODS_INCLUDE_SHIFT(code)) {
			mods |= KC_MOD_SHIFT;
		}

		Term_keypress(code, mods);
	}
}

/**
 * Process events
 */
static errr check_event(bool wait)
{
	struct term *old_term = Term;

	XEvent xev;

	struct x11_term_data *td = NULL;

	int i;
	int window = 0;

	if (x11_event_get(&xev, wait, idle_update) == 1) {
		return 1;
	}

	/* Notice new keymaps */
	if (xev.type == MappingNotify) {
		XRefreshKeyboardMapping(&xev.xmapping);
		return 0;
	}

	/* Scan the windows */
	for (i = 0; i < ANGBAND_TERM_MAX; i++) {
		td = (struct x11_term_data *)angband_term[i]->data;

		if (xev.xany.window == td->win->handle) {
			window = i;
			break;
		}
	}

	/* Unknown window */
	if (!td || !td->win) {
		return 0;
	}

	/* Hack -- activate the Term */
	Term_activate(angband_term[window]);

	/* Switch on the Type */
	switch (xev.type) {
		case ButtonPress: {
			char button = 0;

			/* Where is the mouse */
			int x = xev.xbutton.x;
			int y = xev.xbutton.y;

			if ((xev.xbutton.button >= Button1) &&
				(xev.xbutton.button <= Button5)) {
				button = (char)xev.xbutton.button;
			}

			/* Translate the pixel coordinate to an Angband term coordinate */
			x11_pixel_to_square(X11_TERM_DATA, &x, &y);

			Term_mousepress(x, y, button);

			break;
		}

		case KeyPress: {
			/* Hack -- use "old" term */
			Term_activate(old_term);

			/* Process the key */
			react_keypress(&xev.xkey);

			break;
		}

		case Expose: {
			int x1, x2, y1, y2;

			x1 = (xev.xexpose.x - td->win->ox) / td->tile_width;
			x2 = (xev.xexpose.x + xev.xexpose.width - td->win->ox) /
				td->tile_width;

			y1 = (xev.xexpose.y - td->win->oy) / td->tile_height;
			y2 = (xev.xexpose.y + xev.xexpose.height - td->win->oy) /
				td->tile_height;

			Term_redraw_section(x1, y1, x2, y2);

			break;
		}

		/* Move and/or Resize */
		case ConfigureNotify: {
			int cols, rows, wid, hgt, force_resize;

			int ox = td->win->ox;
			int oy = td->win->oy;

			/* Save the new Window Parms */
			td->win->x = xev.xconfigure.x;
			td->win->y = xev.xconfigure.y;
			td->win->w = xev.xconfigure.width;
			td->win->h = xev.xconfigure.height;

			/* Determine "proper" number of rows/cols */
			cols = ((td->win->w - (ox + ox)) / td->tile_width);
			rows = ((td->win->h - (oy + oy)) / td->tile_height);

			/* Hack -- minimal size */
			if (cols < 1) {
				cols = 1;
			}

			if (rows < 1) {
				rows = 1;
			}

			if (window == 0) {
				/* Hack the main window must be at least 80x24 */
				force_resize = false;
				if (cols < 80) {
					cols = 80;
					force_resize = true;
				}
				if (rows < 24) {
					rows = 24;
					force_resize = true;
				}

				/* Resize the windows if any "change" is needed */
				if (force_resize) {
					/* Desired size of window */
					wid = cols * td->tile_width + (ox + ox);
					hgt = rows * td->tile_height + (oy + oy);

					/* Resize window */
					x11_window_resize(td, wid, hgt);
				}
			}

			/* Resize the Term (if needed) */
			Term_resize(cols, rows);

			break;
		}
	}

	/* Hack -- Activate the old term */
	Term_activate(old_term);

	return 0;
}

/**
 * Handle "activation" of a term
 */
static errr x11_term_xtra_level(int v)
{
	/* Handle "activate" */
	if (v) {
		;
	}

	/* Success */
	return 0;
}

/**
 * React to changes
 */
static errr x11_term_xtra_react(void)
{
	if (x11_display_is_color()) {
		/* Check the colors */
		x11_map_colors();

	}

	/* Handle "arg_graphics" */
	if (arg_graphics != GRAPHICS_NONE) {
		if (!x11_init_tileset()) {
			/* Warning */
			plog("Cannot initialize graphics!");

			/* Cannot enable */
			arg_graphics = GRAPHICS_NONE;
		} else {
			use_graphics = arg_graphics;

			/* TODO: Horrible hack */
			if (!X11_TERM_0_DATA) {
				plog("No Term 0 Data");
			}

			plog_fmt("");

			X11_TERM_0_DATA->tile_height = tileset.tile_height;
			X11_TERM_0_DATA->tile_width = tileset.tile_width;
			X11_TERM_0_DATA->tile_width2 = tileset.tile_width;


			/* Reset visuals */
			reset_visuals(true);
		}
	}

	return 0;
}

/**
 * Erase some characters.
 */
static errr x11_term_wipe(int x, int y, int n)
{
	/* Mega-Hack -- Erase some space */
	x11_font_text_non(X11_TERM_DATA,
					  COLOUR_DARK,
					  x,
					  y,
					  L"",
					  n);

	/* Success */
	return 0;
}

/**
 * Draw some textual characters.
 */
static errr x11_term_text(int x, int y, int n, int a, const wchar_t *s)
{
	/* Draw the text */
	x11_font_text_std(X11_TERM_DATA,
					  a,
					  COLOUR_DARK,
					  x,
					  y,
					  s,
					  n);

	return 0;
}

/**
 * Low level graphics.  Assumes valid input.
 *
 * Draw an array of "special" attr/char pairs at the given location.
 *
 * We use the "Term_pict_win()" function for "graphic" data, which are
 * encoded by setting the "high-bits" of both the "attr" and the "char"
 * data.  We use the "attr" to represent the "row" of the main bitmap,
 * and the "char" to represent the "col" of the main bitmap.  The use
 * of this function is induced by the "higher_pict" flag.
 *
 * If "graphics" is not available, we simply "wipe" the given grids.
 */
static errr x11_term_pict(int x,
						  int y,
						  int n,
						  const int *ap,
						  const wchar_t *cp,
						  const int *tap,
						  const wchar_t *tcp)
{
	int i;
	int x1, y1, w1, h1;
	int x2, y2, w2, h2;

	/* Size of bitmap cell */
	w1 = tileset.tile_width;
	h1 = tileset.tile_height;
	w2 = tileset.tile_width;
	h2 = tileset.tile_height;

	/* Location of window cell */
	x2 = x * w2; // td->size_ow1;
	y2 = y * h2; // td->size_oh1;

	/* Draw attr/char pairs */
	for (i = n-1; i >= 0; i--, x2 -= w2) {
		int a = ap[i];
		wchar_t c = cp[i];

		/* Extract picture */
		int row = (a & 0x7F);
		int col = (c & 0x7F);

		/* Location of bitmap cell */
		x1 = col * w1;
		y1 = row * h1;

		x11_draw_tile(X11_TERM_DATA,
					  tileset.ximage,
					  x1,
					  y1,
					  x2,
					  y2,
					  w2,
					  h2);
	}

	return 0;

}

/**
 * Handle a "special request"
 */
static errr x11_term_xtra(int n, int v)
{
	/* Handle a subset of the legal requests */
	switch (n) {
		/* Make a noise */
		case TERM_XTRA_NOISE: {
			x11_display_do_beep();
			return 0;
		}

		/* Flush the output XXX XXX */
		case TERM_XTRA_FRESH: {
			x11_display_update(1, 0, 0);
			return 0;
		}

		/* Process random events XXX */
		case TERM_XTRA_BORED: {
			return check_event(0);
		}

		/* Process Events XXX */
		case TERM_XTRA_EVENT: {
			return check_event(v);
		}

		/* Flush the events XXX */
		case TERM_XTRA_FLUSH: {
			while (!check_event(false)) {
				;
			}

			return 0;
		}

		/* Handle change in the "level" */
		case TERM_XTRA_LEVEL: {
			return x11_term_xtra_level(v);
		}

		/* Clear the screen */
		case TERM_XTRA_CLEAR: {
			x11_window_wipe(X11_TERM_DATA);
			 return 0;
		}

		/* Delay for some milliseconds */
		case TERM_XTRA_DELAY: {
			if (v > 0) {
				usleep(1000 * v);
			}

			return 0;
		}

		/* React to changes */
		case TERM_XTRA_REACT: {
			return x11_term_xtra_react();
		}
	}

	/* Unknown */
	return 1;
}

static int x11_term_curs(int x, int y)
{
	return x11_draw_curs(X11_TERM_DATA, x, y);
}

static int x11_term_bigcurs(int x, int y)
{
	return x11_draw_bigcurs(X11_TERM_DATA, x, y);
}

void x11_term_install_hooks(struct term *t)
{
	/* Hooks */
	t->xtra_hook = x11_term_xtra;
	t->curs_hook = x11_term_curs;
	t->bigcurs_hook = x11_term_bigcurs;
	t->wipe_hook = x11_term_wipe;
	t->text_hook = x11_term_text;

	/* Use "Term_pict" for "graphic" data */
	if (arg_graphics) {
		t->pict_hook = x11_term_pict;
		t->higher_pict = true;
	}
}


