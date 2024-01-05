#include <assert.h>

#include "visflags.h"
#include "utils/mymalloc.h"
#include "utils/pad.h"

/* TODO this alloc can be rolled up with the rest in bsp_parse.c */
void new_visflags(model_basic_t* worldmap, visflags_t out_arr[MAX_VISFLAGS], const size_t cnt) {
	/* Count Size */
	size_t total_size = 0;
	for (u32 i=0; i<cnt; i++) {
		for (u32 mi=0; mi<worldmap->mesh_cnt; mi++) {
			const u32 face_cnt = worldmap->meshes[mi].face_cnt;
			assert(face_cnt > 0);
			total_size += pad_inc_count(bitarr_get_size(face_cnt), 64);
		}
	}

	if (total_size == 0)
		return;

	/* Malloc */
	char* ptr = myrealloc(out_arr[0].mesh[0], total_size);

	/* Setup */
	for (u32 i=0; i<cnt; i++) {
		visflags_t* out = &out_arr[i];
		for (u32 mi=0; mi<worldmap->mesh_cnt; mi++) {
			const u32 face_cnt = worldmap->meshes[mi].face_cnt;
			out->mesh[mi] = pad_inc_ptr(&ptr, bitarr_get_size(face_cnt), 64);
		}

		for (u32 mi=0; mi<worldmap->mesh_cnt; mi++) {
			const u32 face_cnt = worldmap->meshes[mi].face_cnt;
			const size_t byte_size = bitarr_get_size(face_cnt);
			memset(out->mesh[mi], 0, byte_size);
		}
	}
}
