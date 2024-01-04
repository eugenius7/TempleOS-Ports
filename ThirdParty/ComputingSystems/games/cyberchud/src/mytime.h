#ifndef MYTIME_H
#define MYTIME_H

#ifdef TOSLIKE
#include <stdint.h>
#define TIME_TYPE uint64_t
#else
#include <time.h>
#define TIME_TYPE struct timespec
#endif

#ifdef TOSLIKE
float time_diff(TIME_TYPE start, TIME_TYPE end);
#else
static inline double time_diff(TIME_TYPE start, TIME_TYPE end) {
	end.tv_sec -= start.tv_sec;
	end.tv_nsec -= start.tv_nsec;
	return (double)end.tv_sec + (double)end.tv_nsec*0.000000001;
}
#endif

#ifdef TOSLIKE
TIME_TYPE get_time();
#else
static inline TIME_TYPE get_time() {
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	return ts;
}
#endif

#endif
