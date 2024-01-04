#include "libm.h"

#ifdef __x86_64__
double sqrt(double x) {
	__asm__ ("sqrtsd %1, %0" : "=x"(x) : "x"(x));
	return x;
}

float sqrtf(float x) {
	__asm__ ("sqrtss %1, %0" : "=x"(x) : "x"(x));
	return x;
}

double fabs(double x) {
	double t;
	__asm__ ("pcmpeqd %0, %0" : "=x"(t));          // t = ~0
	__asm__ ("psrlq   $1, %0" : "+x"(t));          // t >>= 1
	__asm__ ("andps   %1, %0" : "+x"(x) : "x"(t)); // x &= t
	return x;
}

float fabsf(float x) {
	float t;
	__asm__ ("pcmpeqd %0, %0" : "=x"(t));          // t = ~0
	__asm__ ("psrld   $1, %0" : "+x"(t));          // t >>= 1
	__asm__ ("andps   %1, %0" : "+x"(x) : "x"(t)); // x &= t
	return x;
}
#else
float fabsf(float x) {
	union {float f; uint32_t i;} u = {x};
	u.i &= 0x7fffffff;
	return u.f;
}
#endif

int abs(int a) {
	return a>0 ? a : -a;
}

double ldexp(double x, int n) {
	return scalbn(x, n);
}

static const double_t toint = 1/EPS;
double ceil(double x) {
	union {double f; uint64_t i;} u = {x};
	int e = u.i >> 52 & 0x7ff;
	double_t y;

	if (e >= 0x3ff+52 || x == 0)
		return x;
	/* y = int(x) - x, where int(x) is an integer neighbor of x */
	if (u.i >> 63)
		y = x - toint + toint - x;
	else
		y = x + toint - toint - x;
	/* special case because of non-nearest rounding modes */
	if (e <= 0x3ff-1) {
		FORCE_EVAL(y);
		return u.i >> 63 ? -0.0 : 1;
	}
	if (y < 0)
		return x + y + 1;
	return x + y;
}

double floor(double x) {
	union {double f; uint64_t i;} u = {x};
	int e = u.i >> 52 & 0x7ff;
	double_t y;

	if (e >= 0x3ff+52 || x == 0)
		return x;
	/* y = int(x) - x, where int(x) is an integer neighbor of x */
	if (u.i >> 63)
		y = x - toint + toint - x;
	else
		y = x + toint - toint - x;
	/* special case because of non-nearest rounding modes */
	if (e <= 0x3ff-1) {
		FORCE_EVAL(y);
		return u.i >> 63 ? -1 : 0;
	}
	if (y > 0)
		return x + y - 1;
	return x + y;
}

float ceilf(float x) {
	union {float f; uint32_t i;} u = {x};
	int e = (int)(u.i >> 23 & 0xff) - 0x7f;
	uint32_t m;

	if (e >= 23)
		return x;
	if (e >= 0) {
		m = 0x007fffff >> e;
		if ((u.i & m) == 0)
			return x;
		FORCE_EVAL(x + 0x1p120f);
		if (u.i >> 31 == 0)
			u.i += m;
		u.i &= ~m;
	} else {
		FORCE_EVAL(x + 0x1p120f);
		if (u.i >> 31)
			u.f = -0.0;
		else if (u.i << 1)
			u.f = 1.0;
	}
	return u.f;
}
