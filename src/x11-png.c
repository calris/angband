/*---------------------------------------------------------------------------

   rpng - simple PNG display program                              readpng.c

  ---------------------------------------------------------------------------

      Copyright (c) 1998-2007 Greg Roelofs.  All rights reserved.

      This software is provided "as is," without warranty of any kind,
      express or implied.  In no event shall the author or contributors
      be held liable for any damages arising in any way from the use of
      this software.

      The contents of this file are DUAL-LICENSED.  You may modify and/or
      redistribute this software according to the terms of one of the
      following two licenses (at your option):


      LICENSE 1 ("BSD-like with advertising clause"):

      Permission is granted to anyone to use this software for any purpose,
      including commercial applications, and to alter it and redistribute
      it freely, subject to the following restrictions:

      1. Redistributions of source code must retain the above copyright
         notice, disclaimer, and this list of conditions.
      2. Redistributions in binary form must reproduce the above copyright
         notice, disclaimer, and this list of conditions in the documenta-
         tion and/or other materials provided with the distribution.
      3. All advertising materials mentioning features or use of this
         software must display the following acknowledgment:

            This product includes software developed by Greg Roelofs
            and contributors for the book, "PNG: The Definitive Guide,"
            published by O'Reilly and Associates.


      LICENSE 2 (GNU GPL v2 or later):

      This program is free software; you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
      the Free Software Foundation; either version 2 of the License, or
      (at your option) any later version.

      This program is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      GNU General Public License for more details.

      You should have received a copy of the GNU General Public License
      along with this program; if not, write to the Free Software Foundation,
      Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  ---------------------------------------------------------------------------*/

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <png.h>

#include "h-basic.h"
#include "z-virt.h"
#include "z-util.h"
#include "z-form.h"
#include "x11-util.h"
#include "x11-png.h"

/*
 * TODO: These are a total hack - preset & hard coded Alpha and tile background
 * colours - I should be ashamed of myself for doing all this work and not
 * finishing it off properly
 */
#define LUT_EXPONENT		1.0
#define CRT_EXPONENT		2.2
#define BACKGROUND_RED		0
#define BACKGROUND_GREEN	0
#define BACKGROUND_BLUE		0


struct x11_png_image {
	FILE *file;
	png_structp png_ptr;
	png_infop info_ptr;
	byte *image_data;

	u32b width;
	u32b height;

	double display_exponent;
	int channels;
	u32b bytes_per_row;

	int bit_depth;

	int red_shift;
	int green_shift;
	int blue_shift;
};

static bool x11_png_read_init(x11_png_image_t *png_image, const char *filename);
static bool x11_png_read_image(x11_png_image_t *png_image);
static void x11_png_read_cleanup(x11_png_image_t *png_image,
								 bool free_image_data);

int x11_png_get_bgcolor(x11_png_image_t *png_image,
						byte *bg_red,
						byte *bg_green,
						byte *bg_blue);

int  bit_depth, color_type;

x11_png_image_t *x11_png_image_init(void) {
	x11_png_image_t *png_image;

	png_image = (x11_png_image_t *)mem_zalloc(sizeof(x11_png_image_t));

	png_image->display_exponent = LUT_EXPONENT * CRT_EXPONENT;

	return png_image;
}

void x11_png_image_nuke(x11_png_image_t *png_image)
{
	if (!png_image) {
		return;
	}

	if (png_image->file) {
		fclose(png_image->file);
		png_image->file = NULL;
	}

	if (png_image->image_data) {
		mem_free(png_image->image_data);
		png_image->image_data = NULL;
	}
}

bool x11_png_image_load(x11_png_image_t *png_image, const char *filename)
{
	if (!x11_png_read_init(png_image, filename)){
		return false;
	}

	if (!x11_png_read_image(png_image)) {
		return false;
	}

	plog_fmt("Successfully read %s", filename);

	x11_png_read_cleanup(png_image, false);

	return true;

}

static bool x11_png_read_init(x11_png_image_t *png_image, const char *filename)
{
	byte sig[8];

	if (!(png_image->file = fopen(filename, "rb"))) {
		plog_fmt("can't open PNG file [%s]\n", filename);

		return false;
	}

	/*
	 * first do a quick check that the file really is a PNG image; could
	 * have used slightly more general png_sig_cmp() function instead
	 */

	fread(sig, 1, 8, png_image->file);

	if (!png_check_sig(sig, 8)) {
		plog_fmt("[%s] is not a PNG file: incorrect signature\n", filename);
		return false;
	}

	/* could pass pointers to user-defined error handlers instead of NULLs: */
	png_image->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
												NULL,
												NULL,
												NULL);

	if (!png_image->png_ptr) {
		plog("insufficient memory\n");
		return false;
	}

	png_image->info_ptr = png_create_info_struct(png_image->png_ptr);

	if (!png_image->info_ptr) {
		png_destroy_read_struct(&png_image->png_ptr, NULL, NULL);
		plog("insufficient memory\n");
		return false;
	}

	/*
	 * we could create a second info struct here (end_info), but it's only
	 * useful if we want to keep pre- and post-IDAT chunk info separated
	 * (mainly for PNG-aware image editors and converters)
	 */

	/*
	 * setjmp() must be called in every function that calls a PNG-reading
	 * libpng function
	 */
	if (setjmp(png_jmpbuf(png_image->png_ptr))) {
		plog_fmt("[%s] has bad IHDR (libpng longjmp)\n", filename);
		return 2;
	}

	png_init_io(png_image->png_ptr, png_image->file);
	png_set_sig_bytes(png_image->png_ptr, 8);

	/* read all PNG info up to image data */
	png_read_info(png_image->png_ptr, png_image->info_ptr);


	/*
	 * alternatively, could make separate calls to png_get_image_width(),
	 * etc., but want bit_depth and color_type for later [don't care about
	 * compression_type and filter_type => NULLs]
	 */
	png_get_IHDR(png_image->png_ptr,
				 png_image->info_ptr,
				 &png_image->width,
				 &png_image->height,
				 &bit_depth,
				 &color_type,
				 NULL,
				 NULL,
				 NULL);

	return true;
}

/* returns 0 if succeeds, 1 if fails due to no bKGD chunk, 2 if libpng error;
 * scales values to 8-bit if necessary */

int x11_png_get_bgcolor(x11_png_image_t *png_image,
						byte *red,
						byte *green,
						byte *blue)
{
	png_color_16p pBackground;


	/*
	 * setjmp() must be called in every function that calls a PNG-reading
	 * libpng function
	 */
	if (setjmp(png_jmpbuf(png_image->png_ptr))) {
		png_destroy_read_struct(&png_image->png_ptr,
								&png_image->info_ptr,
								NULL);
		return 2;
	}


	if (!png_get_valid(png_image->png_ptr,
					   png_image->info_ptr,
					   PNG_INFO_bKGD)) {
		return 1;
	}

	/*
	 * It is not obvious from the libpng documentation, but this function
	 * takes a pointer to a pointer, and it always returns valid red, green
	 * and blue values, regardless of color_type:
	 */
	png_get_bKGD(png_image->png_ptr, png_image->info_ptr, &pBackground);


	/*
	 * however, it always returns the raw bKGD data, regardless of any
	 * bit-depth transformations, so check depth and adjust if necessary
	 */
	if (bit_depth == 16) {
		*red   = pBackground->red   >> 8;
		*green = pBackground->green >> 8;
		*blue  = pBackground->blue  >> 8;
	} else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth < 8)) {
		if (bit_depth == 1) {
			*red = *green = *blue = pBackground->gray? 255 : 0;
		} else if (bit_depth == 2) {
			*red = *green = *blue = (255/3) * pBackground->gray;
		} else {
			/* bit_depth == 4 */
			*red = *green = *blue = (255/15) * pBackground->gray;
		}
	} else {
		*red   = (byte)pBackground->red;
		*green = (byte)pBackground->green;
		*blue  = (byte)pBackground->blue;
	}

	return 0;
}

bool x11_png_read_image(x11_png_image_t *png_image)
{
	size_t image_data_size;
	double  gamma;
	png_uint_32  i; //, rowbytes;
	png_bytepp row_pointers = NULL;

	/*
	 * setjmp() must be called in every function that calls a PNG-reading
	 * libpng function
	 */
	if (setjmp(png_jmpbuf(png_image->png_ptr))) {
		png_destroy_read_struct(&png_image->png_ptr,
								&png_image->info_ptr,
								NULL);
		return false;
	}

	/*
	 * Expand palette images to RGB, low-bit-depth grayscale images to 8 bits,
	 * transparency chunks to full alpha channel; strip 16-bit-per-sample
	 * images to 8 bits per sample; and convert grayscale to RGB[A]
	 */
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_expand(png_image->png_ptr);
	}

	if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth < 8)) {
		png_set_expand(png_image->png_ptr);
	}

	if (png_get_valid(png_image->png_ptr, png_image->info_ptr, PNG_INFO_tRNS)) {
		png_set_expand(png_image->png_ptr);
	}

	if (bit_depth == 16) {
		png_set_strip_16(png_image->png_ptr);
	}

	if ((color_type == PNG_COLOR_TYPE_GRAY) ||
		(color_type == PNG_COLOR_TYPE_GRAY_ALPHA)) {
		png_set_gray_to_rgb(png_image->png_ptr);
	}

	/*
	 * Unlike the example in the libpng documentation, we have *no* idea where
	 * this file may have come from--so if it doesn't have a file gamma, don't
	 * do any correction ("do no harm")
	 */
	if (png_get_gAMA(png_image->png_ptr, png_image->info_ptr, &gamma)) {
		png_set_gamma(png_image->png_ptr, png_image->display_exponent, gamma);
	}

	/*
	 * All transformations have been registered; now update info_ptr data,
	 * get rowbytes and channels, and allocate image memory
	 */
	png_read_update_info(png_image->png_ptr, png_image->info_ptr);

	png_image->bytes_per_row = png_get_rowbytes(png_image->png_ptr,
												png_image->info_ptr);

	png_image->channels = (int)png_get_channels(png_image->png_ptr,
												png_image->info_ptr);

	image_data_size = png_image->bytes_per_row * png_image->height;

	png_image->image_data = (byte *)mem_alloc(image_data_size);
	row_pointers = (png_bytepp)mem_alloc(png_image->height * sizeof(png_bytep));

	plog_fmt("x11_png_read_image: channels = %d, bytes_per_row = %ld, height = %ld",
			 png_image->channels,
			 png_image->bytes_per_row,
			 png_image->height);

	/* set the individual row_pointers to point at the correct offsets */

	for (i = 0;  i < png_image->height;  ++i) {
		row_pointers[i] = png_image->image_data  + i * png_image->bytes_per_row;
	}

	png_read_image(png_image->png_ptr, row_pointers);

	/*
	 * and we're done!  (png_read_end() can be omitted if no processing of
	 * post-IDAT text/time/etc. is desired)
	 */
	mem_free(row_pointers);
	png_read_end(png_image->png_ptr, NULL);

	return true;
}

static void x11_png_read_cleanup(x11_png_image_t *png_image,
								 bool free_image_data)
{
	if (free_image_data && png_image->image_data) {
		free(png_image->image_data);
		png_image->image_data = NULL;
	}

	if (png_image->png_ptr && png_image->info_ptr) {
		png_destroy_read_struct(&png_image->png_ptr,
								&png_image->info_ptr,
								NULL);
		png_image->png_ptr = NULL;
		png_image->info_ptr = NULL;
	}
}


static int rpng_x_msb(unsigned long val)
{
	int i;

	for (i = 31;  i >= 0;  --i) {
		if (val & 0x80000000L) {
			break;
		}

		val <<= 1;
	}

	return i;
}

static bool x11_png_calculate_shifts(x11_png_image_t *png_image)
{
	unsigned int depth = x11_display_depth();
	unsigned long red_mask = x11_visual_red_mask();
	unsigned long green_mask = x11_visual_green_mask();
	unsigned long blue_mask = x11_visual_blue_mask();

	if (depth == 15 || depth == 16) {
		/* these are right-shifts */
		png_image->red_shift = 15 - rpng_x_msb(red_mask);
		png_image->green_shift = 15 - rpng_x_msb(green_mask);
		png_image->blue_shift = 15 - rpng_x_msb(blue_mask);
	} else if (depth > 16) {
		/* these are left-shifts */
		png_image->red_shift = rpng_x_msb(red_mask) - 7;
		png_image->green_shift = rpng_x_msb(green_mask) - 7;
		png_image->blue_shift = rpng_x_msb(blue_mask) - 7;
	}

	if (depth >= 15 && (png_image->red_shift < 0 ||
						png_image->green_shift < 0 ||
						png_image->blue_shift < 0)) {
		plog("PNG internal logic error:  negative X shift(s)!");
		return false;
	}

	return true;
}

XImage *x11_png_create_ximage(x11_png_image_t *png_image)
{
	XImage *ximage;
	char *xdata;
	int pad;
	unsigned int depth = x11_display_depth();
	unsigned long i, row;
	byte *src;
	char *dest;
	byte r, g, b, a;
	unsigned long pixel;
	unsigned long red_mask = x11_visual_red_mask();
	unsigned long green_mask = x11_visual_green_mask();
	unsigned long blue_mask = x11_visual_blue_mask();

	if (!x11_png_calculate_shifts(png_image)) {
		return NULL;
	}

	if (depth == 24 || depth == 32) {
		xdata = (char *)mem_alloc(4 * png_image->width * png_image->height);
		pad = 32;
	} else if (depth == 16) {
		xdata = (char *)mem_alloc(2 * png_image->width * png_image->height);
		pad = 16;
	} else if (depth == 8) {
		xdata = (char *)mem_alloc(png_image->width * png_image->height);
		pad = 8;

		plog_fmt("Unsupported display depth: %i", depth);

		return NULL;
} else {
		plog_fmt("Unsupported display depth: %i", depth);

		return NULL;
	}

	ximage = x11_ximage_init(ZPixmap,
							 0,
							 xdata,
							 png_image->width,
							 png_image->height,
							 pad,
							 0);

	if (!ximage) {
		plog("Failed to create XImage");
		mem_free(xdata);

		return NULL;
	}

	/*
	 * To avoid testing the byte order every pixel (or doubling the size of
	 * the drawing routine with a giant if-test), we arbitrarily set the byte
	 * order to MSBFirst and let Xlib worry about inverting things on little-
	 * endian machines (like Linux/x86, old VAXen, etc.)--this is not the most
	 * efficient approach (the giant if-test would be better), but in the
	 * interest of clarity, we take the easy way out...
	 */
	ximage->byte_order = MSBFirst;

	plog("Converting PNG to XImage");
	plog_fmt("    (png_image->channels      == %d)", png_image->channels);
	plog_fmt("    (png_image->bytes_per_row == %d)", png_image->bytes_per_row);
	plog_fmt("    (png_image->width         == %d)", png_image->width);
	plog_fmt("    (png_image->height        == %d)", png_image->height);
	plog_fmt("    (ximage->bytes_per_line   == %d)",
			 ximage->bytes_per_line);
	plog_fmt("    (ximage->bits_per_pixel   == %d)",
			 ximage->bits_per_pixel);
	plog_fmt("    (ximage->byte_order       == %s)",
			ximage->byte_order == MSBFirst ? "MSBFirst" :
					 (ximage->byte_order == LSBFirst ? "LSBFirst" : "unknown"));

	if ((png_image->channels != 3) && (png_image->channels != 4)) {
		plog("Unsupported number of channels");
		mem_free(ximage->data);
		XDestroyImage(ximage);

		return NULL;
	}

	if (depth == 24 || depth == 32) {
		unsigned long red, green, blue;

		for (row = 0;  row < png_image->height;  ++row) {
			src = png_image->image_data + row * png_image->bytes_per_row;
			dest = ximage->data + row * ximage->bytes_per_line;

			if (png_image->channels == 3) {
				for (i = png_image->width;  i > 0;  --i) {
					red   = *src++;
					green = *src++;
					blue  = *src++;

					pixel = (red   << png_image->red_shift) |
							(green << png_image->green_shift) |
							(blue  << png_image->blue_shift);
					/* recall that we set ximage->byte_order = MSBFirst above */
					/* GRR BUG:  this assumes bpp == 32, but may be 24: */
					*dest++ = (char)((pixel >> 24) & 0xff);
					*dest++ = (char)((pixel >> 16) & 0xff);
					*dest++ = (char)((pixel >>  8) & 0xff);
					*dest++ = (char)( pixel        & 0xff);
				}
			} else { /* png_image->channels == 4 */
				for (i = png_image->width;  i > 0;  --i) {
					r = *src++;
					g = *src++;
					b = *src++;
					a = *src++;

					if (a == 255) {
						red   = r;
						green = g;
						blue  = b;
					} else if (a == 0) {
						red   = BACKGROUND_RED;
						green = BACKGROUND_GREEN;
						blue  = BACKGROUND_BLUE;
					} else {
						/*
						 * this macro (from png.h) composites the foreground
						 * and background values and puts the result into the
						 * first argument
						 */
						png_composite(red,   r, a, BACKGROUND_RED);
						png_composite(green, g, a, BACKGROUND_GREEN);
						png_composite(blue,  b, a, BACKGROUND_BLUE);
					}

					pixel = (red   << png_image->red_shift) |
							(green << png_image->green_shift) |
							(blue  << png_image->blue_shift);

					/* recall that we set ximage->byte_order = MSBFirst above */
					*dest++ = (char)((pixel >> 24) & 0xff);
					*dest++ = (char)((pixel >> 16) & 0xff);
					*dest++ = (char)((pixel >>  8) & 0xff);
					*dest++ = (char)( pixel        & 0xff);
				}
			}
		}

	} else if (depth == 16) {
		unsigned short red, green, blue;

		for (row = 0;  row < png_image->height;  ++row) {
			src = png_image->image_data + row * png_image->bytes_per_row;
			dest = ximage->data + row * ximage->bytes_per_line;

			if (png_image->channels == 3) {
				for (i = png_image->width;  i > 0;  --i) {
					red   = ((unsigned short)(*src) << 8);
					++src;
					green = ((unsigned short)(*src) << 8);
					++src;
					blue  = ((unsigned short)(*src) << 8);
					++src;

					pixel = ((red   >> png_image->red_shift) & red_mask) |
							((green >> png_image->green_shift) & green_mask) |
							((blue  >> png_image->blue_shift) & blue_mask);

					/* recall that we set ximage->byte_order = MSBFirst above */
					*dest++ = (char)((pixel >>  8) & 0xff);
					*dest++ = (char)( pixel        & 0xff);
				}
			} else { /* png_image->channels == 4 */
				for (i = png_image->width;  i > 0;  --i) {
					r = *src++;
					g = *src++;
					b = *src++;
					a = *src++;

					if (a == 255) {
						red   = ((unsigned short)r << 8);
						green = ((unsigned short)g << 8);
						blue  = ((unsigned short)b << 8);
					} else if (a == 0) {
						red   = ((unsigned short)BACKGROUND_RED   << 8);
						green = ((unsigned short)BACKGROUND_GREEN << 8);
						blue  = ((unsigned short)BACKGROUND_BLUE  << 8);
					} else {
						/*
						 * This macro (from png.h) composites the foreground
						 * and background values and puts the result back into
						 * the first argument (== fg byte here:  safe)
						 */
						png_composite(r, r, a, BACKGROUND_RED);
						png_composite(g, g, a, BACKGROUND_GREEN);
						png_composite(b, b, a, BACKGROUND_BLUE);
						red   = ((unsigned short)r << 8);
						green = ((unsigned short)g << 8);
						blue  = ((unsigned short)b << 8);
					}
					pixel = ((red   >> png_image->red_shift) & red_mask) |
							((green >> png_image->green_shift) & green_mask) |
							((blue  >> png_image->blue_shift) & blue_mask);
					/* recall that we set ximage->byte_order = MSBFirst above */
					*dest++ = (char)((pixel >>  8) & 0xff);
					*dest++ = (char)( pixel        & 0xff);
				}
			}
		}

	} else { /* depth == 8 */
		/* GRR:  add 8-bit support */
		plog_fmt("Display depth %d not supported", depth);

		return NULL;
	}

	return ximage;
}

