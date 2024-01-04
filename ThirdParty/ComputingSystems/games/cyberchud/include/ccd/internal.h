#ifndef __CCD_INTERNAL_H__
#define __CCD_INTERNAL_H__

#include "ccd/vec3.h"

extern ccd_vec3_t* ccd_vec3_origin;
ccd_real_t ccdVec3PointTriDist2(const ccd_vec3_t *P,
                                const ccd_vec3_t *x0, const ccd_vec3_t *B,
                                const ccd_vec3_t *C,
                                ccd_vec3_t *witness);

#endif
