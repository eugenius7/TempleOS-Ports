/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "bsp.h"
#include "text.h"
#include "utils/myds.h"

static uint8_t* Mod_DecompressVis(const uint8_t* in, const bsp_qmodel_t* const model) {
	static uint8_t *mod_decompressed = NULL;

	int row = (model->numleafs + 31) / 8;
	if (mod_decompressed == NULL || row > arrlen(mod_decompressed)) {
		arrsetlen(mod_decompressed, row);
		if (!mod_decompressed)
			myprintf("Mod_DecompressVis: realloc() failed on %d bytes\n", row);
	}
	uint8_t* out = mod_decompressed;
	uint8_t* outend = mod_decompressed + row;

	if (!in) { // no vis info, so make all visible
		while (row) {
			*out++ = 0xff;
			row--;
		}
		return mod_decompressed;
	}

	do {
		if (*in) {
			*out++ = *in++;
			continue;
		}

		int c = in[1];
		in += 2;
		if (c > row - (out - mod_decompressed))
			c = row - (out - mod_decompressed); // now that we're dynamically allocating pvs buffers, we have to be more careful to avoid heap overflows with buggy maps.
		while (c) {
			if (out == outend) {
				myprintf("Mod_DecompressVis: output overrun on model\n");
				return mod_decompressed;
			}
			*out++ = 0;
			c--;
		}
	} while (out - mod_decompressed < row);

	return mod_decompressed;
}

uint8_t* Mod_LeafPVS(const bsp_mleaf_t* const leaf, const bsp_qmodel_t* const model) {
#ifndef NDEBUG
	if (leaf == model->leafs)
		myprintf("[Mod_LeafPVS] leaf == model->leafs\n");
#endif
	/* if (leaf == model->leafs) */
	/* 	return Mod_NoVisPVS (model); */
	return Mod_DecompressVis(leaf->compressed_vis, model);
}
