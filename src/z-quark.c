/**
 * \file z-quark.c
 * \brief Save memory by storing strings in a global array, ensuring
 * that each is only allocated once.
 *
 * Copyright (c) 1997 Ben Harrison
 * Copyright (c) 2007 "Elly"
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
#include "z-virt.h"
#include "z-util.h"
#include "z-quark.h"
#include "init.h"

typedef struct {
	char *str;
	u32b hash;
}quark;

static quark *quarks;
static size_t nr_quarks = 1;
static size_t alloc_quarks = 0;

#define QUARKS_INIT	16

quark_t quark_add(const char *str)
{
	u32b hash = djb2_hash(str);

	quark_t q;

	for (q = 1; q < nr_quarks; q++) {
		if (quarks[q].hash == hash)
			if (!strcmp(quarks[q].str, str))
				return q;
	}

	if (nr_quarks == alloc_quarks) {
		alloc_quarks *= 2;
		quarks = mem_realloc(quarks, alloc_quarks * sizeof(quark));
	}

	q = nr_quarks++;
	quarks[q].hash = hash;
	quarks[q].str = string_make(str);

	return q;
}

const char *quark_str(quark_t q)
{
	return (q >= nr_quarks ? NULL : quarks[q].str);
}

void quarks_init(void)
{
	alloc_quarks = QUARKS_INIT;
	quarks = mem_zalloc(alloc_quarks * sizeof(quark));
}

void quarks_free(void)
{
	size_t i;

	/* quarks[0] is special */
	for (i = 1; i < nr_quarks; i++)
		string_free(quarks[i].str);

	mem_free(quarks);
}

struct init_module z_quark_module = {
	.name = "z-quark",
	.init = quarks_init,
	.cleanup = quarks_free
};
