/**
 * \file main-x11.c
 * \brief Provide support for the X Windowing System
 *
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */
#include <png.h>

#include "angband.h"
#include "buildid.h"
#include "game-world.h"
#include "init.h"
#include "ui-display.h"
#include "ui-game.h"
#include "ui-prefs.h"
#include "grafmode.h"
#include "x11-png.h"

#include "x11-util.h"

static bool init_graphics(void);

struct x11_tileset tileset;

/**
 * This file helps Angband work with UNIX/X11 computers.
 *
 * To use this file, compile with "USE_X11" defined, and link against all
 * the various "X11" libraries which may be needed.
 *
 * Part of this file provides a user interface package composed of several
 * pseudo-objects, including "metadpy" (a display), "infowin" (a window),
 * "infoclr" (a color), and "infofnt" (a font).  Actually, the package was
 * originally much more interesting, but it was bastardized to keep this
 * file simple.
 *
 * The rest of this file is an implementation of "main-xxx.c" for X11.
 *
 * Most of this file is by Ben Harrison (benh@phial.com).
 */

/**
 * The following shell script can be used to launch Angband, assuming that
 * it was extracted into "~/Angband", and compiled using "USE_X11", on a
 * Linux machine, with a 1280x1024 screen, using 6 windows (with the given
 * characteristics), with gamma correction of 1.8 -> (1 / 1.8) * 256 = 142,
 * and without graphics (add "-g" for graphics).  Just copy this comment
 * into a file, remove the leading " * " characters (and the head/tail of
 * this comment), and make the file executable.
 *
 *
 * #!/bin/csh
 *
 * # Describe attempt
 * echo "Launching angband..."
 * sleep 2
 *
 * # Main window
 * setenv ANGBAND_X11_FONT_0 10x20
 * setenv ANGBAND_X11_AT_X_0 5
 * setenv ANGBAND_X11_AT_Y_0 510
 *
 * # Message window
 * setenv ANGBAND_X11_FONT_1 8x13
 * setenv ANGBAND_X11_AT_X_1 5
 * setenv ANGBAND_X11_AT_Y_1 22
 * setenv ANGBAND_X11_ROWS_1 35
 *
 * # Inventory window
 * setenv ANGBAND_X11_FONT_2 8x13
 * setenv ANGBAND_X11_AT_X_2 635
 * setenv ANGBAND_X11_AT_Y_2 182
 * setenv ANGBAND_X11_ROWS_2 23
 *
 * # Equipment window
 * setenv ANGBAND_X11_FONT_3 8x13
 * setenv ANGBAND_X11_AT_X_3 635
 * setenv ANGBAND_X11_AT_Y_3 22
 * setenv ANGBAND_X11_ROWS_3 12
 *
 * # Monster recall window
 * setenv ANGBAND_X11_FONT_4 6x13
 * setenv ANGBAND_X11_AT_X_4 817
 * setenv ANGBAND_X11_AT_Y_4 847
 * setenv ANGBAND_X11_COLS_4 76
 * setenv ANGBAND_X11_ROWS_4 11
 *
 * # Object recall window
 * setenv ANGBAND_X11_FONT_5 6x13
 * setenv ANGBAND_X11_AT_X_5 817
 * setenv ANGBAND_X11_AT_Y_5 520
 * setenv ANGBAND_X11_COLS_5 76
 * setenv ANGBAND_X11_ROWS_5 24
 *
 * # The build directory
 * cd ~/Angband
 *
 * # Gamma correction
 * setenv ANGBAND_X11_GAMMA 142
 *
 * # Launch Angband
 * ./src/angband -mx11 -- -n6 &
 *
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>

#include "main.h"

/*
 * Default fonts (when using X11).
 */
#define DEFAULT_X11_FONT		"9x15"
#define DEFAULT_X11_FONT_0		"10x20"
#define DEFAULT_X11_FONT_1		"9x15"
#define DEFAULT_X11_FONT_2		"9x15"
#define DEFAULT_X11_FONT_3		"5x8"
#define DEFAULT_X11_FONT_4		"5x8"
#define DEFAULT_X11_FONT_5		"5x8"
#define DEFAULT_X11_FONT_6		"5x8"
#define DEFAULT_X11_FONT_7		"5x8"

/**
 * Notes on Colors:
 *
 *   1) On a monochrome (or "fake-monochrome") display, all colors
 *   will be "cast" to "fg," except for the bg color, which is,
 *   obviously, cast to "bg".  Thus, one can ignore this setting.
 *
 *   2) Because of the inner functioning of the color allocation
 *   routines, colors may be specified as (a) a typical color name,
 *   (b) a hexidecimal color specification (preceded by a pound sign),
 *   or (c) by strings such as "fg", "bg", "zg".
 *
 *   3) Due to the workings of the init routines, many colors
 *   may also be dealt with by their actual pixel values.  Note that
 *   the pixel with all bits set is "zg = (1<<metadpy->depth)-1", which
 *   is not necessarily either black or white.
 */

/*
 * Actual color table
 */
static struct x11_color *clr[MAX_COLORS * BG_MAX];

static bool gamma_table_ready;
static int gamma_val;

/**
 * Color info (unused, red, green, blue).
 */
static byte color_table_x11[MAX_COLORS][4];

/**
 * Path to the X11 settings file
 */
static const char *x11_prefs = "x11-settings.prf";
static char settings[1024];

/**
 * Remember the number of terminal windows open
 */
static int term_windows_open;



#define X11_TERM_DATA			((struct x11_term_data *)Term->data)
#define X11_TERM_0_DATA			((struct x11_term_data *)angband_term[0]->data)

/**
 * Hack -- Convert an RGB value to an X11 Pixel, or die.
 */
static u32b create_pixel(byte red, byte green, byte blue)
{
	XColor xcolor;

	if (!gamma_table_ready) {
		const char *str = getenv("ANGBAND_X11_GAMMA");

		if (str) {
			gamma_val = atoi(str);
		}

		gamma_table_ready = true;

		/* Only need to build the table if gamma exists */
		if (gamma_val) {
			build_gamma_table(gamma_val);
		}
	}

	/* Hack -- Gamma Correction */
	if (gamma_val > 0) {
		red = gamma_table[red];
		green = gamma_table[green];
		blue = gamma_table[blue];
	}

	/* Build the color */
	xcolor.red   = red * 257;
	xcolor.green = green * 257;
	xcolor.blue  = blue * 257;
	xcolor.flags = DoRed | DoGreen | DoBlue;

	/* Attempt to Allocate the Parsed color */
	if (!x11_color_allocate(&xcolor)) {
		quit_fmt("Couldn't allocate bitmap color #%04x%04x%04x\n",
				 xcolor.red,
				 xcolor.green,
				 xcolor.blue);
	}

	return xcolor.pixel;
}

/**
 * Get the name of the default font to use for the term.
 */
static const char *get_default_font(int term_num)
{
	const char *font;
	char buf[80];

	/* Window specific font name */
	strnfmt(buf, sizeof(buf), "ANGBAND_X11_FONT_%d", term_num);

	/* Check environment for that font */
	font = getenv(buf);

	if (font) {
		return font;
	}

	/* Check environment for "base" font */
	font = getenv("ANGBAND_X11_FONT");

	if (font) {
		return font;
	}

	switch (term_num) {
		case 0: {
			return DEFAULT_X11_FONT_0;
		}

		case 1: {
			return DEFAULT_X11_FONT_1;
		}

		case 2: {
			return DEFAULT_X11_FONT_2;
		}

		case 3: {
			return DEFAULT_X11_FONT_3;
		}

		case 4: {
			return DEFAULT_X11_FONT_4;
		}

		case 5: {
			return DEFAULT_X11_FONT_5;
		}

		case 6: {
			return DEFAULT_X11_FONT_6;
		}

		case 7: {
			return DEFAULT_X11_FONT_7;
		}
	}

	return DEFAULT_X11_FONT;
}

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
	int i;

	if (x11_display_is_color()) {
		/* Check the colors */
		for (i = 0; i < MAX_COLORS; i++) {
			if ((color_table_x11[i][0] != angband_color_table[i][0]) ||
				(color_table_x11[i][1] != angband_color_table[i][1]) ||
				(color_table_x11[i][2] != angband_color_table[i][2]) ||
				(color_table_x11[i][3] != angband_color_table[i][3])) {
				pixell pixel;

				/* Save new values */
				color_table_x11[i][0] = angband_color_table[i][0];
				color_table_x11[i][1] = angband_color_table[i][1];
				color_table_x11[i][2] = angband_color_table[i][2];
				color_table_x11[i][3] = angband_color_table[i][3];

				/* Create pixel */
				pixel = create_pixel(color_table_x11[i][1],
									 color_table_x11[i][2],
									 color_table_x11[i][3]);

				/* Change the foreground */
				x11_color_change_fg(clr[i], pixel);
			}
		}
	}

	/* Handle "arg_graphics" */
	if (arg_graphics != GRAPHICS_NONE) {
		if (!init_graphics()) {
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
 * Initialize graphics
 */
static bool init_graphics(void)
{
	/* Initialize once */
	char buf[1024];
	graphics_mode *mode = NULL;

	x11_png_image_t *png_image;

	if (arg_graphics) {
		mode = get_graphics_mode(arg_graphics);
	}

	if (!mode) {
		mode = get_graphics_mode(1);
	}

	if (mode) {
		if (!mode->pref[0]) {
			plog_fmt("invalid tile prefname '%s'", mode->menuname);
			return false;
		}

		tileset.tile_width = mode->cell_width;
		tileset.tile_height = mode->cell_height;

		if ((tileset.tile_width < 2) || (tileset.tile_height < 2)) {
			plog_fmt("invalid tile dimensions in tileset: '%s'",
					 mode->menuname);
			return false;
		}

		tileset.path = mode->path;
		tileset.name = mode->file;
		tileset.overdraw = mode->overdrawRow;
		tileset.overdrawmax = mode->overdrawMax;
		tileset.alphablend = mode->alphablend;

		current_graphics_mode = mode;
	} else {
		plog("could not find graphics mode");
		return false;
	}

	/* Access the bitmap file */
	path_build(buf, sizeof(buf), tileset.path, tileset.name);

	png_image = x11_png_image_init();

	if (!png_image) {
		arg_graphics = 0;
		use_graphics = 0;
	}

	/* Load the image or quit */
	if (tileset.alphablend) {
			char modname[1024];
			my_strcpy(modname, buf,1024);

			plog_fmt("Load alphablend tileset from: %s", buf);

			if (x11_png_image_load(png_image, buf)) {
				plog_fmt("Successfuly loaded tileset");
			}

			tileset.ximage = x11_png_create_ximage(png_image);

			if (tileset.ximage) {
				plog_fmt("Successfuly converted tiles to ximage");
			}
	} else {
		plog_fmt("Load non-alphablend tileset from: %s", buf);

		if (x11_png_image_load(png_image, buf)) {
			plog_fmt("Successfuly loaded tileset");
		}

		tileset.ximage = x11_png_create_ximage(png_image);

		if (tileset.ximage) {
			plog_fmt("Successfuly converted tiles to ximage");
		}
	}

	return arg_graphics;
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

/**
 * Erase some characters.
 */
static errr x11_term_wipe(int x, int y, int n)
{
	/* Mega-Hack -- Erase some space */
	x11_font_text_non(X11_TERM_DATA,
					  clr[COLOUR_DARK],
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
					  clr[a],
					  clr[COLOUR_DARK],
					  x,
					  y,
					  s,
					  n);

	return 0;
}

static void save_prefs(void)
{
	ang_file *fff;
	int i;

	/* Open the settings file */
	fff = file_open(settings, MODE_WRITE, FTYPE_TEXT);

	if (!fff) {
		return;
	}

	/* Header */
	file_putf(fff, "# %s X11 settings\n\n", VERSION_NAME);

	/* Number of term windows to open */
	file_putf(fff, "TERM_WINS=%d\n\n", term_windows_open);

	/* Save window prefs */
	for (i = 0; i < ANGBAND_TERM_MAX; i++) {
		struct x11_term_data *td;

		if (angband_term[i]) {
			td = (struct x11_term_data *)angband_term[i]->data;

			if (!angband_term[i]->mapped_flag) {
				continue;
			}

			/* Header */
			file_putf(fff, "# Term %d\n", i);

			/*
			 * This doesn't seem to work under various WMs
			 * since the decoration messes the position up
			 *
			 * Hack -- Use saved window positions.
			 * This means that we won't remember ingame repositioned
			 * windows, but also means that WMs won't screw predefined
			 * positions up. -CJN-
			 */

			/* Window specific location (x) */
			file_putf(fff, "AT_X_%d=%d\n", i, td->win->x);

			/* Window specific location (y) */
			file_putf(fff, "AT_Y_%d=%d\n", i, td->win->y);

			/* Window specific cols */
			file_putf(fff, "COLS_%d=%d\n", i, angband_term[i]->wid);

			/* Window specific rows */
			file_putf(fff, "ROWS_%d=%d\n", i, angband_term[i]->hgt);

			/* Window specific inner border offset (ox) */
			file_putf(fff, "IBOX_%d=%d\n", i, td->win->ox);

			/* Window specific inner border offset (oy) */
			file_putf(fff, "IBOY_%d=%d\n", i, td->win->oy);

			/* Window specific font name */
			file_putf(fff, "FONT_%d=%s\n", i, td->font->name);

			/* Window specific tile width */
			file_putf(fff, "TILE_WIDTH_%d=%d\n", i, td->tile_width);

			/* Window specific tile height */
			file_putf(fff, "TILE_HEIGHT_%d=%d\n", i, td->tile_height);

			/* Footer */
			file_putf(fff, "\n");
		}
	}

	/* Close */
	file_close(fff);
}

static int x11_term_curs(int x, int y)
{
	return x11_draw_curs(X11_TERM_DATA, x, y);
}

static int x11_term_bigcurs(int x, int y)
{
	return x11_draw_bigcurs(X11_TERM_DATA, x, y);
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
 * Initialize a term_data
 */
static errr term_data_init(struct term *t, int i)
{
	const char *name = angband_term_name[i];

	const char *font;

	int x = 0;
	int y = 0;

	int cols = 80;
	int rows = 24;

	int ox = 1;
	int oy = 1;

	int wid, hgt, num;

	const char *str;

	int val;

	XClassHint *ch;

	char res_name[20];
	char res_class[20];

	XSizeHints *sh;

	ang_file *fff;

	char buf[1024];
	char cmd[40];
	char font_name[256];

	int line = 0;

	struct x11_term_data *td = mem_zalloc(sizeof(struct x11_term_data));

	t->data = (void *)td;

	/* Get default font for this term */
	font = get_default_font(i);

	/* Open the file */
	fff = file_open(settings, MODE_READ, FTYPE_TEXT);

	/* File exists */
	if (fff) {
		/* Process the file */
		while (file_getl(fff, buf, sizeof(buf))) {
			/* Count lines */
			line++;

			/* Skip "empty" lines */
			if (!buf[0]) {
				continue;
			}

			/* Skip "blank" lines */
			if (isspace((unsigned char)buf[0])) {
				continue;
			}

			/* Skip comments */
			if (buf[0] == '#') {
				continue;
			}

			/* Window specific location (x) */
			strnfmt(cmd, sizeof(cmd), "AT_X_%d", i);

			if (prefix(buf, cmd)) {
				str = strstr(buf, "=");
				x = str ? atoi(str + 1) : -1;

				continue;
			}

			/* Window specific location (y) */
			strnfmt(cmd, sizeof(cmd), "AT_Y_%d", i);

			if (prefix(buf, cmd)) {
				str = strstr(buf, "=");
				y = str ? atoi(str + 1) : -1;

				continue;
			}

			/* Window specific cols */
			strnfmt(cmd, sizeof(cmd), "COLS_%d", i);

			if (prefix(buf, cmd)) {
				str = strstr(buf, "=");
				val = str ? atoi(str + 1) : -1;

				if (val > 0) {
					cols = val;
				}

				continue;
			}

			/* Window specific rows */
			strnfmt(cmd, sizeof(cmd), "ROWS_%d", i);

			if (prefix(buf, cmd)) {
				str = strstr(buf, "=");
				val = str ? atoi(str + 1) : -1;

				if (val > 0) {
					rows = val;
				}

				continue;
			}

			/* Window specific inner border offset (ox) */
			strnfmt(cmd, sizeof(cmd), "IBOX_%d", i);

			if (prefix(buf, cmd)) {
				str = strstr(buf, "=");
				val = str ? atoi(str + 1) : -1;

				if (val > 0) {
					ox = val;
				}

				continue;
			}

			/* Window specific inner border offset (oy) */
			strnfmt(cmd, sizeof(cmd), "IBOY_%d", i);

			if (prefix(buf, cmd)) {
				str = strstr(buf, "=");
				val = str ? atoi(str + 1) : -1;

				if (val > 0) {
					oy = val;
				}

				continue;
			}

			/* Window specific font name */
			strnfmt(cmd, sizeof(cmd), "FONT_%d", i);

			if (prefix(buf, cmd)) {
				str = strstr(buf, "=");

				if (str) {
					my_strcpy(font_name, str + 1, sizeof(font_name));
					font = font_name;
				}

				continue;
			}

			/* Window specific tile width */
			strnfmt(cmd, sizeof(cmd), "TILE_WIDTH_%d", i);

			if (prefix(buf, cmd)) {
				str = strstr(buf, "=");
				val = str ? atoi(str + 1) : -1;

				if (val > 0) {
					td->tile_width = val;
				}

				continue;
			}

			/* Window specific tile height */
			strnfmt(cmd, sizeof(cmd), "TILE_HEIGHT_%d", i);

			if (prefix(buf, cmd)) {
				str = strstr(buf, "=");
				val = str ? atoi(str + 1) : -1;

				if (val > 0) {
					td->tile_height = val;
				}

				continue;
			}
		}

		/* Close */
		file_close(fff);
	}

	/*
	 * Env-vars overwrite the settings in the settings file
	 */

	/* Window specific location (x) */
	strnfmt(buf, sizeof(buf), "ANGBAND_X11_AT_X_%d", i);
	str = getenv(buf);
	val = str ? atoi(str) : -1;

	if (val > 0) {
		x = val;
	}

	/* Window specific location (y) */
	strnfmt(buf, sizeof(buf), "ANGBAND_X11_AT_Y_%d", i);
	str = getenv(buf);
	val = str ? atoi(str) : -1;

	if (val > 0) {
		y = val;
	}

	/* Window specific cols */
	strnfmt(buf, sizeof(buf), "ANGBAND_X11_COLS_%d", i);
	str = getenv(buf);
	val = str ? atoi(str) : -1;

	if (val > 0) {
		cols = val;
	}

	/* Window specific rows */
	strnfmt(buf, sizeof(buf), "ANGBAND_X11_ROWS_%d", i);
	str = getenv(buf);
	val = str ? atoi(str) : -1;

	if (val > 0) {
		rows = val;
	}

	/* Window specific inner border offset (ox) */
	strnfmt(buf, sizeof(buf), "ANGBAND_X11_IBOX_%d", i);
	str = getenv(buf);
	val = str ? atoi(str) : -1;

	if (val > 0) {
		ox = val;
	}

	/* Window specific inner border offset (oy) */
	strnfmt(buf, sizeof(buf), "ANGBAND_X11_IBOY_%d", i);
	str = getenv(buf);
	val = str ? atoi(str) : -1;

	if (val > 0) {
		oy = val;
	}

	/* Window specific font name */
	strnfmt(buf, sizeof(buf), "ANGBAND_X11_FONT_%d", i);
	str = getenv(buf);

	if (str) {
		font = str;
	}

	/* Hack the main window must be at least 80x24 */
	if (!i) {
		if (cols < 80) {
			cols = 80;
		}

		if (rows < 24) {
			rows = 24;
		}
	}

	/* Prepare the standard font */
	td->font = mem_zalloc(sizeof(struct x11_font));

	if (x11_font_init(td->font, font)) {
		quit_fmt("Couldn't load the requested font. (%s)", font);
	}

	/* Use proper tile size */
	if (td->tile_width <= 0) {
		td->tile_width = td->font->width;
	}

	if (td->tile_height <= 0) {
		td->tile_height = td->font->height;
	}

	/* Don't allow bigtile mode - one day maybe NRM */
	td->tile_width2 = td->tile_width;

	/* Hack -- key buffer size */
	num = ((i == 0) ? 1024 : 16);

	/* Assume full size windows */
	wid = cols * td->tile_width + (ox + ox);
	hgt = rows * td->tile_height + (oy + oy);

	x11_window_init(td, x, y, wid, hgt, 0);

	/* Ask for certain events */
	x11_window_set_mask(td,
						ExposureMask |
						StructureNotifyMask |
						KeyPressMask |
						ButtonPressMask);

	/* Set the window name */
	x11_window_set_name(td, name);

	/* Save the inner border */
	x11_window_set_border(td, ox, oy);

	/* Make Class Hints */
	ch = XAllocClassHint();

	if (!ch) {
		quit("XAllocClassHint failed");
	}

	my_strcpy(res_name, name, sizeof(res_name));
	res_name[0] = tolower((unsigned char)res_name[0]);
	ch->res_name = res_name;

	my_strcpy(res_class, "Angband", sizeof(res_class));
	ch->res_class = res_class;

	x11_window_set_class_hint(td, ch);

	/* Make Size Hints */
	sh = XAllocSizeHints();

	/* Oops */
	if (!sh) {
		quit("XAllocSizeHints failed");
	}

	if (x || y)
		sh->flags = USPosition;
	else
		sh->flags = 0;

	/* Main window has a differing minimum size */
	if (i == 0) {
		/* Main window min size is 80x24 */
		sh->flags |= (PMinSize | PMaxSize);
		sh->min_width = 80 * td->tile_width + (ox + ox);
		sh->min_height = 24 * td->tile_height + (oy + oy);
		sh->max_width = 255 * td->tile_width + (ox + ox);
		sh->max_height = 255 * td->tile_height + (oy + oy);
	} else {
		/* Other windows */
		sh->flags |= (PMinSize | PMaxSize);
		sh->min_width = td->tile_width + (ox + ox);
		sh->min_height = td->tile_height + (oy + oy);
		sh->max_width = 255 * td->tile_width + (ox + ox);
		sh->max_height = 255 * td->tile_height + (oy + oy);
	}

	/* Resize increment */
	sh->flags |= PResizeInc;
	sh->width_inc = td->tile_width;
	sh->height_inc = td->tile_height;

	/* Base window size */
	sh->flags |= PBaseSize;
	sh->base_width = (ox + ox);
	sh->base_height = (oy + oy);

	/* Use the size hints */
	x11_window_set_size_hints(td, sh);

	/* Map the window */
	x11_window_map(td);

	/* Set pointers to allocated data */
	td->sizeh = sh;
	td->classh = ch;

	/* Move the window to requested location */
	if ((x >= 0) && (y >= 0)) {
		x11_window_move(td, x, y);
	}

	/* Initialize the term */
	term_init(t, cols, rows, num);

	/* Use a "soft" cursor */
	t->soft_cursor = true;

	/* Erase with "white space" */
	t->attr_blank = COLOUR_WHITE;
	t->char_blank = ' ';

	/* Differentiate between BS/^h, Tab/^i, etc. */
	t->complex_input = true;

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


	/* Save the data */
	t->data = td;

	/* Activate (important) */
	Term_activate(t);

	/* Success */
	return 0;
}

const char help_x11[] = "Basic X11, subopts -d<display> -n<windows> -x<file>";

static void hook_quit(const char *str)
{
	int i;

	/* Unused */
	(void)str;

	save_prefs();

	/* Free allocated data */
	for (i = 0; i < term_windows_open; i++) {
		struct x11_term_data *td;

		td = (struct x11_term_data *)angband_term[i]->data;

		/* Free size hints */
		XFree(td->sizeh);

		/* Free class hints */
		XFree(td->classh);

		/* Free fonts */
		x11_font_nuke(td->font);
		mem_free(td->font);

		/* Free window */
		x11_window_nuke(td);

		mem_free(td);

		/* Free term */
		term_nuke(angband_term[i]);

		mem_free(angband_term[i]);
	}

	/* Free colors */
	x11_free_cursor_col();

	for (i = 0; i < MAX_COLORS * BG_MAX; ++i) {
		x11_color_nuke(clr[i]);
		mem_free(clr[i]);
	}

	/* Close link to display */
	x11_display_nuke();
}

/*
 * Initialization function for an "X11" module to Angband
 */
errr init_x11(int argc, char **argv)
{
	int i;
	const char *display_name = "";
	int num_term = -1;
	ang_file *fff;
	char buf[1024];
	const char *str;
	int val;
	int line = 0;

	/* Parse args */
	for (i = 1; i < argc; i++) {
		if (prefix(argv[i], "-d")) {
			display_name = &argv[i][2];
			continue;
		}

		if (prefix(argv[i], "-n")) {
			num_term = atoi(&argv[i][2]);

			if (num_term > ANGBAND_TERM_MAX) {
				num_term = ANGBAND_TERM_MAX;
			} else if (num_term < 1) {
				num_term = 1;
			}

			continue;
		}

		if (prefix(argv[i], "-x")) {
			x11_prefs = argv[i] + 2;
			continue;
		}

		plog_fmt("Ignoring option: %s", argv[i]);
	}

	/* Build the filename */
	path_build(settings,
			   sizeof(settings),
			   ANGBAND_DIR_USER,
			   x11_prefs);

	plog(settings);

	if (num_term == -1) {
		num_term = 1;

		/* Open the file */
		fff = file_open(settings, MODE_READ, FTYPE_TEXT);

		/* File exists */
		if (fff) {
			/* Process the file */
			while (file_getl(fff, buf, sizeof(buf))) {
				/* Count lines */
				line++;

				/* Skip "empty" lines */
				if (!buf[0]) {
					continue;
				}

				/* Skip "blank" lines */
				if (isspace((unsigned char)buf[0])) {
					continue;
				}

				/* Skip comments */
				if (buf[0] == '#') {
					continue;
				}

				/* Number of terminal windows */
				if (prefix(buf, "TERM_WINS")) {
					str = strstr(buf, "=");
					val = str ? atoi(str + 1) : -1;

					if (val > 0) {
						num_term = val;
					}

					continue;
				}
			}

			/* Close */
			file_close(fff);
		}
	}

	/* load the possible graphics modes */
	if (!init_graphics_modes()) {
		plog_fmt("Graphics list load failed");
	}

	/* Init the x11_display if possible */
	if (x11_display_init(display_name)) {
		return -1;
	}

	/* Remember the number of terminal windows */
	term_windows_open = num_term;

	/* Prepare cursor color */
	x11_alloc_cursor_col();

	/* Prepare normal colors */
	for (i = 0; i < MAX_COLORS * BG_MAX; ++i) {
		pixell pixel;

		clr[i] = mem_zalloc(sizeof(struct x11_color));

		/* Acquire Angband colors */
		color_table_x11[i % MAX_COLORS][0] =
			angband_color_table[i % MAX_COLORS][0];
		color_table_x11[i % MAX_COLORS][1] =
			angband_color_table[i % MAX_COLORS][1];
		color_table_x11[i % MAX_COLORS][2] =
			angband_color_table[i % MAX_COLORS][2];
		color_table_x11[i % MAX_COLORS][3] =
			angband_color_table[i % MAX_COLORS][3];

		/* Default to monochrome */
		pixel = ((i == 0) ? x11_display_color_bg() : x11_display_color_fg());

		/*
		 * Handle color
		 * This block of code has added support for background colors
		 * (from Sil)
		 */
		if (x11_display_is_color()) {
			pixell bp;

			/* Create pixel */
			pixel = create_pixel(color_table_x11[i % MAX_COLORS][1],
								 color_table_x11[i % MAX_COLORS][2],
								 color_table_x11[i % MAX_COLORS][3]);

			switch (i / MAX_COLORS) {
				case BG_BLACK: {
					/* Default Background */
					x11_color_init(clr[i],
								   pixel,
								   x11_display_color_bg(),
								   CPY,
								   0);
					break;
				}

				case BG_SAME: {
					/* Background same as foreground */
					bp = create_pixel(color_table_x11[i % MAX_COLORS][1],
									  color_table_x11[i % MAX_COLORS][2],
									  color_table_x11[i % MAX_COLORS][3]);

					x11_color_init(clr[i],
								   pixel,
								   bp,
								   CPY,
								   0);
					break;
				}

				case BG_DARK: {
					/* Highlight Background */
					bp = create_pixel(color_table_x11[COLOUR_SHADE][1],
									  color_table_x11[COLOUR_SHADE][2],
									  color_table_x11[COLOUR_SHADE][3]);

					x11_color_init(clr[i],
								   pixel,
								   bp,
								   CPY,
								   0);
					break;
				}
			}
		} else {
			/* Handle monochrome */
			x11_color_init(clr[i], pixel, x11_display_color_bg(), CPY, 0);
		}
	}

	/* Initialize the windows */
	for (i = 0; i < num_term; i++) {
		angband_term[i] = mem_zalloc(sizeof(struct term));

		/* Initialize the term_data */
		term_data_init(angband_term[i], i);
	}

	/* Raise the "Angband" window */
	x11_window_raise(((struct x11_term_data *)angband_term[0]->data));

	/* Activate the "Angband" window screen */
	Term_activate(angband_term[0]);

	/* Activate hook */
	quit_aux = hook_quit;

	return 0;
}

