/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_extrude.c
 *  \ingroup bmesh
 *
 * Extrude faces and solidify.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_buffer.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

enum {
	EXT_INPUT   = 1,
	EXT_KEEP    = 2,
	EXT_DEL     = 4
};

void bmo_create_dimension_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMVert *v1,*v2;
	BMDim *d;

	v1 = BMO_iter_new(&siter, op->slots_in, "verts", BM_VERT);
	v2 = BMO_iter_step(&siter);

	if (v1 && v2) {
		d = BM_dim_create(bm, v1, v2, NULL, BM_CREATE_NOP);

		BMO_elem_flag_enable(bm, d, EXT_KEEP);
		BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "dim.out", BM_DIM, EXT_KEEP);

	}
}
