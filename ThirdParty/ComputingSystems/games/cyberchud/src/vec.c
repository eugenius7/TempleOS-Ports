#include <assert.h>
#include <stdio.h>

#include "vec.h"
#include "text.h"
#include "utils/minmax.h"

void print_vec3(const char* const str, const vec3s vec) {
	myprintf("%s%.2fx%.2fx%.2f\n", str, vec.x, vec.y, vec.z);
}

void print_vec2(const char* const str, const vec2s vec) {
	myprintf("%s %.2fx%.2f\n", str, vec.x, vec.y);
}

void print_mat3(const char* const str, const mat3s* const mat) {
	fputs(str, stdout);
	for (int i=0; i<3; i++) {
		print_vec3("", mat->col[i]);
	}
}

void get_vectors_from_angles(const vec3s angles, mat3s* vectors) {
	/* Front */
	const float cosf_y = cosf(angles.y);
	vectors->col[0].x = cosf(angles.x) * cosf_y;
	vectors->col[0].y = sinf(angles.y);
	vectors->col[0].z = sinf(angles.x) * cosf_y;
	vectors->col[0] = glms_vec3_normalize(vectors->col[0]);
	/* Right */
	vectors->col[1] = glms_vec3_normalize(glms_vec3_cross(vectors->col[0], (vec3s){.x=0,.y=1,.z=0}));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
	/* Up */
	vectors->col[2] = glms_vec3_normalize(glms_vec3_cross(vectors->col[1], vectors->col[0]));
}
