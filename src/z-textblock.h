/**
 * \file z-textblock.h
 * \brief Text output bugger (?NRM) code
 *
 * Copyright (c) 2010 Andi Sidwell
 * Copyright (c) 2011 Peter Denison
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 */

#ifndef INCLUDED_Z_TEXTBLOCK_H
#define INCLUDED_Z_TEXTBLOCK_H

#include "z-file.h"

struct textblock *textblock_new(void);

void textblock_free(struct textblock *tb);

void textblock_append(struct textblock *tb, const char *fmt, ...);

void textblock_append_c(struct textblock *tb, byte attr, const char *fmt, ...);

void textblock_append_pict(struct textblock *tb, byte attr, int c);

void textblock_append_utf8(struct textblock *tb, const char *utf8_string);

const wchar_t *textblock_text(struct textblock *tb);

const byte *textblock_attrs(struct textblock *tb);

size_t textblock_calculate_lines(struct textblock *tb,
				 size_t **line_starts,
				 size_t **line_lengths,
				 size_t width);

void textblock_to_file(struct textblock *tb,
		       ang_file *f,
		       int indent,
		       int wrap_at);

extern ang_file *text_out_file;

extern void (*text_out_hook)(byte a, const char *str);

extern int text_out_wrap;

extern int text_out_indent;

extern int text_out_pad;

extern void text_out_to_file(byte attr, const char *str);

extern void text_out(const char *fmt, ...);

extern void text_out_c(byte a, const char *fmt, ...);

extern void text_out_e(const char *fmt, ...);

typedef void (*text_writer)(ang_file *f);

errr text_lines_to_file(const char *path, text_writer writer);

#endif /* INCLUDED_Z_TEXTBLOCK_H */
