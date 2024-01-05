#ifndef STOLENLIB_H
#define STOLENLIB_H

#include <float.h>
#include <stddef.h>
#include <stdint.h>

double floor(double x);
double cos(double x);
float cosf(float x);
float acosf(float x);
double atan(double x);
float atanf(float x);
double atan2(double y, double x);
float atan2f(float y, float x);
float asinf(float x);
float fminf(float x, float y);
float fmodf(float x, float y);
float powf(float x, float y);
double sin(double x);
float sinf(float x);
void sincosf(float x, float *sin, float *cos);
void qsort (void *, size_t, size_t, int (*)(const void *, const void *));
float tanf(float x);
int memcmp(const void *vl, const void *vr, size_t n);
int strcmp(const char *l, const char *r);
size_t strlen(const char *s);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);

#endif
