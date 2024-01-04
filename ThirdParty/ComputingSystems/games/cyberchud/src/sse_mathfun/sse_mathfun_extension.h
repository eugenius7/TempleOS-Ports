/*
sse_mathfun_extension.h - zlib license
Written by Tolga Mizrak 2016
Extension of sse_mathfun.h, which is written by Julien Pommier

Based on the corresponding algorithms of the cephes math library

This is written as an extension to sse_mathfun.h instead of modifying it, just because I didn't want
to maintain a modified version of the original library. This way switching to a newer version of the
library won't be a hassle.

Note that non SSE2 implementations of tan_ps, atan_ps, cot_ps and atan2_ps are not implemented yet.
As such, currently you need to #define USE_SSE2 to compile.

With tan_ps, cot_ps you get good precision on input ranges that are further away from the domain
borders (-PI/2, PI/2 for tan and 0, 1 for cot). See the results on the deviations for these
functions on my machine:
checking tan on [-0.25*Pi, 0.25*Pi]
max deviation from tanf(x): 1.19209e-07 at 0.250000006957*Pi, max deviation from cephes_tan(x):
5.96046e-08
   ->> precision OK for the tan_ps <<-

checking tan on [-0.49*Pi, 0.49*Pi]
max deviation from tanf(x): 3.8147e-06 at -0.490000009841*Pi, max deviation from cephes_tan(x):
9.53674e-07
   ->> precision OK for the tan_ps <<-

checking cot on [0.2*Pi, 0.7*Pi]
max deviation from cotf(x): 1.19209e-07 at 0.204303119606*Pi, max deviation from cephes_cot(x):
1.19209e-07
   ->> precision OK for the cot_ps <<-

checking cot on [0.01*Pi, 0.99*Pi]
max deviation from cotf(x): 3.8147e-06 at 0.987876517942*Pi, max deviation from cephes_cot(x):
9.53674e-07
   ->> precision OK for the cot_ps <<-

With atan_ps and atan2_ps you get pretty good precision, atan_ps max deviation is < 2e-7 and
atan2_ps max deviation is < 2.5e-7
*/

/* Copyright (C) 2016 Tolga Mizrak

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
	 claim that you wrote the original software. If you use this software
	 in a product, an acknowledgment in the product documentation would be
	 appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
	 misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  (this is the zlib license)
*/

#pragma once

#ifndef _SSE_MATHFUN_EXTENSION_H_INCLUDED_
#define _SSE_MATHFUN_EXTENSION_H_INCLUDED_

#ifndef USE_SSE2
#error sse1 & mmx version not implemented
#endif

#ifdef _MSC_VER
#pragma warning( push )
/* warning C4838: conversion from 'double' to 'const float' requires a narrowing conversion */
#pragma warning( disable : 4838 )
/* warning C4305: 'initializing': truncation from 'double' to 'const float' */
#pragma warning( disable : 4305 )
#endif

#include "sse_mathfun.h"

static const float_const ps_0 __attribute__((aligned(16))) = { .raw={0,0,0,0}};
static const float_const ps_2 __attribute__((aligned(16))) = { .raw={2,2,2,2}};
static const float_const pi32_neg1 __attribute__((aligned(16))) = { .raw={1,1,1,1}};

static const float_const tancof_p0 __attribute__((aligned(16))) = { .fraw={9.38540185543E-3,9.38540185543E-3,9.38540185543E-3,9.38540185543E-3}};
static const float_const tancof_p1 __attribute__((aligned(16))) = { .fraw={3.11992232697E-3,3.11992232697E-3,3.11992232697E-3,3.11992232697E-3}};
static const float_const tancof_p2 __attribute__((aligned(16))) = { .fraw={2.44301354525E-2,2.44301354525E-2,2.44301354525E-2,2.44301354525E-2}};
static const float_const tancof_p3 __attribute__((aligned(16))) = { .fraw={5.34112807005E-2,5.34112807005E-2,5.34112807005E-2,5.34112807005E-2}};
static const float_const tancof_p4 __attribute__((aligned(16))) = { .fraw={1.33387994085E-1,1.33387994085E-1,1.33387994085E-1,1.33387994085E-1}};
static const float_const tancof_p5 __attribute__((aligned(16))) = { .fraw={3.33331568548E-1,3.33331568548E-1,3.33331568548E-1,3.33331568548E-1}};

static const float_const tancot_eps __attribute__((aligned(16))) = { .fraw={1.0e-4,1.0e-4,1.0e-4,1.0e-4}};

v4sf tancot_ps( v4sf x, int cotFlag )
{
	v4sf xmm1, xmm2 = _mm_setzero_ps(), xmm3, sign_bit, y;

#ifdef USE_SSE2
	v4si emm2;
#else
#endif
	sign_bit = x;
	/* take the absolute value */
	x = _mm_and_ps( x, inv_sign_mask.v4sf );
	/* extract the sign bit (upper one) */
	sign_bit = _mm_and_ps( sign_bit, sign_mask.v4sf );

	/* scale by 4/Pi */
	y = _mm_mul_ps( x, cephes_FOPI.v4sf );

#ifdef USE_SSE2
	/* store the integer part of y in mm0 */
	emm2 = _mm_cvttps_epi32( y );
	/* j=(j+1) & (~1) (see the cephes sources) */
	emm2 = _mm_add_epi32( emm2, pi32_1.v4si );
	emm2 = _mm_and_si128( emm2, pi32_inv1.v4si );
	y = _mm_cvtepi32_ps( emm2 );

	emm2 = _mm_and_si128( emm2, pi32_2.v4si );
	emm2 = _mm_cmpeq_epi32( emm2, _mm_setzero_si128() );

	v4sf poly_mask = _mm_castsi128_ps( emm2 );
#else
#endif
	/* The magic pass: "Extended precision modular arithmetic"
	   x = ((x - y * DP1) - y * DP2) - y * DP3; */
	xmm1 = minus_cephes_DP1.v4sf;
	xmm2 = minus_cephes_DP2.v4sf;
	xmm3 = minus_cephes_DP3.v4sf;
	xmm1 = _mm_mul_ps( y, xmm1 );
	xmm2 = _mm_mul_ps( y, xmm2 );
	xmm3 = _mm_mul_ps( y, xmm3 );
	v4sf z = _mm_add_ps( x, xmm1 );
	z = _mm_add_ps( z, xmm2 );
	z = _mm_add_ps( z, xmm3 );

	v4sf zz = _mm_mul_ps( z, z );

	y = tancof_p0.v4sf;
	y = _mm_mul_ps( y, zz );
	y = _mm_add_ps( y, tancof_p1.v4sf );
	y = _mm_mul_ps( y, zz );
	y = _mm_add_ps( y, tancof_p2.v4sf );
	y = _mm_mul_ps( y, zz );
	y = _mm_add_ps( y, tancof_p3.v4sf );
	y = _mm_mul_ps( y, zz );
	y = _mm_add_ps( y, tancof_p4.v4sf );
	y = _mm_mul_ps( y, zz );
	y = _mm_add_ps( y, tancof_p5.v4sf );
	y = _mm_mul_ps( y, zz );
	y = _mm_mul_ps( y, z );
	y = _mm_add_ps( y, z );

	v4sf y2;
	if( cotFlag ) {
		y2 = _mm_xor_ps( y, sign_mask.v4sf );
		/* y = _mm_rcp_ps( y ); */
		/* using _mm_rcp_ps here loses on way too much precision, better to do a div */
		y = _mm_div_ps( ps_1.v4sf, y );
	} else {
		/* y2 = _mm_rcp_ps( y ); */
		/* using _mm_rcp_ps here loses on way too much precision, better to do a div */
		y2 = _mm_div_ps( ps_1.v4sf, y );
		y2 = _mm_xor_ps( y2, sign_mask.v4sf );
	}

	/* select the correct result from the two polynoms */
	xmm3 = poly_mask;
	y = _mm_and_ps( xmm3, y );
	y2 = _mm_andnot_ps( xmm3, y2 );
	y = _mm_or_ps( y, y2 );

	/* update the sign */
	y = _mm_xor_ps( y, sign_bit );

	return y;
}

v4sf tan_ps( v4sf x ) { return tancot_ps( x, 0 ); }

v4sf cot_ps( v4sf x ) { return tancot_ps( x, 1 ); }

static const float_const atanrange_hi __attribute__((aligned(16))) = { .fraw={2.414213562373095,2.414213562373095,2.414213562373095,2.414213562373095}};
static const float_const atanrange_lo __attribute__((aligned(16))) = { .fraw={0.4142135623730950,0.4142135623730950,0.4142135623730950,0.4142135623730950}};
static const float PIF = 3.141592653589793238;
static const float PIO2F = 1.5707963267948966192;
static const float_const cephes_PIF __attribute__((aligned(16))) = { .fraw={3.141592653589793238,3.141592653589793238,3.141592653589793238,3.141592653589793238}};
static const float_const cephes_PIO2F __attribute__((aligned(16))) = { .fraw={1.5707963267948966192,1.5707963267948966192,1.5707963267948966192,1.5707963267948966192}};
static const float_const cephes_PIO4F __attribute__((aligned(16))) = { .fraw={0.7853981633974483096,0.7853981633974483096,0.7853981633974483096,0.7853981633974483096}};

static const float_const atancof_p0 __attribute__((aligned(16))) = { .fraw={8.05374449538e-2,8.05374449538e-2,8.05374449538e-2,8.05374449538e-2}};
static const float_const atancof_p1 __attribute__((aligned(16))) = { .fraw={1.38776856032E-1,1.38776856032E-1,1.38776856032E-1,1.38776856032E-1}};
static const float_const atancof_p2 __attribute__((aligned(16))) = { .fraw={1.99777106478E-1,1.99777106478E-1,1.99777106478E-1,1.99777106478E-1}};
static const float_const atancof_p3 __attribute__((aligned(16))) = { .fraw={3.33329491539E-1,3.33329491539E-1,3.33329491539E-1,3.33329491539E-1}};

v4sf atan_ps( v4sf x )
{
	v4sf sign_bit, y;

	sign_bit = x;
	/* take the absolute value */
	x = _mm_and_ps( x, inv_sign_mask.v4sf );
	/* extract the sign bit (upper one) */
	sign_bit = _mm_and_ps( sign_bit, sign_mask.v4sf );

/* range reduction, init x and y depending on range */
#ifdef USE_SSE2
	/* x > 2.414213562373095 */
	v4sf cmp0 = _mm_cmpgt_ps( x, atanrange_hi.v4sf );
	/* x > 0.4142135623730950 */
	v4sf cmp1 = _mm_cmpgt_ps( x, atanrange_lo.v4sf );

	/* x > 0.4142135623730950 && !( x > 2.414213562373095 ) */
	v4sf cmp2 = _mm_andnot_ps( cmp0, cmp1 );

	/* -( 1.0/x ) */
	v4sf y0 = _mm_and_ps( cmp0, cephes_PIO2F.v4sf );
	v4sf x0 = _mm_div_ps( ps_1.v4sf, x );
	x0 = _mm_xor_ps( x0, sign_mask.v4sf );

	v4sf y1 = _mm_and_ps( cmp2, cephes_PIO4F.v4sf );
	/* (x-1.0)/(x+1.0) */
	v4sf x1_o = _mm_sub_ps( x, ps_1.v4sf );
	v4sf x1_u = _mm_add_ps( x, ps_1.v4sf );
	v4sf x1 = _mm_div_ps( x1_o, x1_u );

	v4sf x2 = _mm_and_ps( cmp2, x1 );
	x0 = _mm_and_ps( cmp0, x0 );
	x2 = _mm_or_ps( x2, x0 );
	cmp1 = _mm_or_ps( cmp0, cmp2 );
	x2 = _mm_and_ps( cmp1, x2 );
	x = _mm_andnot_ps( cmp1, x );
	x = _mm_or_ps( x2, x );

	y = _mm_or_ps( y0, y1 );
#else
#error sse1 & mmx version not implemented
#endif

	v4sf zz = _mm_mul_ps( x, x );
	v4sf acc = atancof_p0.v4sf;
	acc = _mm_mul_ps( acc, zz );
	acc = _mm_sub_ps( acc, atancof_p1.v4sf );
	acc = _mm_mul_ps( acc, zz );
	acc = _mm_add_ps( acc, atancof_p2.v4sf );
	acc = _mm_mul_ps( acc, zz );
	acc = _mm_sub_ps( acc, atancof_p3.v4sf );
	acc = _mm_mul_ps( acc, zz );
	acc = _mm_mul_ps( acc, x );
	acc = _mm_add_ps( acc, x );
	y = _mm_add_ps( y, acc );

	/* update the sign */
	y = _mm_xor_ps( y, sign_bit );

	return y;
}

v4sf atan2_ps( v4sf y, v4sf x )
{
	v4sf x_eq_0 = _mm_cmpeq_ps( x, ps_0.v4sf );
	v4sf x_gt_0 = _mm_cmpgt_ps( x, ps_0.v4sf );
	v4sf x_le_0 = _mm_cmple_ps( x, ps_0.v4sf );
	v4sf y_eq_0 = _mm_cmpeq_ps( y, ps_0.v4sf );
	v4sf x_lt_0 = _mm_cmplt_ps( x, ps_0.v4sf );
	v4sf y_lt_0 = _mm_cmplt_ps( y, ps_0.v4sf );

	v4sf zero_mask = _mm_and_ps( x_eq_0, y_eq_0 );
	v4sf zero_mask_other_case = _mm_and_ps( y_eq_0, x_gt_0 );
	zero_mask = _mm_or_ps( zero_mask, zero_mask_other_case );

	v4sf pio2_mask = _mm_andnot_ps( y_eq_0, x_eq_0 );
	v4sf pio2_mask_sign = _mm_and_ps( y_lt_0, sign_mask.v4sf );
	v4sf pio2_result = cephes_PIO2F.v4sf;
	pio2_result = _mm_xor_ps( pio2_result, pio2_mask_sign );
	pio2_result = _mm_and_ps( pio2_mask, pio2_result );

	v4sf pi_mask = _mm_and_ps( y_eq_0, x_le_0 );
	v4sf pi = cephes_PIF.v4sf;
	v4sf pi_result = _mm_and_ps( pi_mask, pi );

	v4sf swap_sign_mask_offset = _mm_and_ps( x_lt_0, y_lt_0 );
	swap_sign_mask_offset = _mm_and_ps( swap_sign_mask_offset, sign_mask.v4sf );

	v4sf offset0 = _mm_setzero_ps();
	v4sf offset1 = cephes_PIF.v4sf;
	offset1 = _mm_xor_ps( offset1, swap_sign_mask_offset );

	v4sf offset = _mm_andnot_ps( x_lt_0, offset0 );
	offset = _mm_and_ps( x_lt_0, offset1 );

	v4sf arg = _mm_div_ps( y, x );
	v4sf atan_result = atan_ps( arg );
	atan_result = _mm_add_ps( atan_result, offset );

	/* select between zero_result, pio2_result and atan_result */

	v4sf result = _mm_andnot_ps( zero_mask, pio2_result );
	atan_result = _mm_andnot_ps( pio2_mask, atan_result );
	atan_result = _mm_andnot_ps( pio2_mask, atan_result);
	result = _mm_or_ps( result, atan_result );
	result = _mm_or_ps( result, pi_result );

	return result;
}

/* for convenience of calling simd sqrt */
float sqrt_ps( float x )
{
	v4sf sse_value = _mm_set_ps1( x );
	sse_value = _mm_sqrt_ps( sse_value );
	return _mm_cvtss_f32( sse_value );
}
float rsqrt_ps( float x )
{
	v4sf sse_value = _mm_set_ps1( x );
	sse_value = _mm_rsqrt_ps( sse_value );
	return _mm_cvtss_f32( sse_value );
}

/* atan2 implementation using atan, used as a reference to implement atan2_ps */
float atan2_ref( float y, float x )
{
	if( x == 0.0f ) {
		if( y == 0.0f ) {
			return 0.0f;
		}
		float result = cephes_PIO2F.fraw[0];
		if( y < 0.0f ) {
			result = -result;
		}
		return result;
	}

	if( y == 0.0f ) {
		if( x > 0.0f ) {
			return 0.0f;
		}
		return PIF;
	}

	float offset = 0;
	if( x < 0.0f ) {
		offset = PIF;
		if( y < 0.0f ) {
			offset = -offset;
		}
	}

	v4sf val = _mm_set_ps1( y / x );
	val = atan_ps( val );
	return offset + _mm_cvtss_f32( val );
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#endif
