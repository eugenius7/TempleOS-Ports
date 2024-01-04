#ifndef THREAD_H
#define THREAD_H

#if PTHREADS
#include <pthread.h>
#define thread_t pthread_t
#define mutex_t pthread_mutex_t
#define cond_t pthread_cond_t
#define LockMutex pthread_mutex_lock
#define UnlockMutex pthread_mutex_unlock
#define CondSignal pthread_cond_signal
#define CondBroadcast pthread_cond_broadcast
#define CondWait pthread_cond_wait
static inline mutex_t* CreateMutex(void) {
	pthread_mutexattr_t attr;
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	mutex_t* mu = calloc(1, sizeof(mutex_t));
	pthread_mutex_init(mu, &attr);
	return mu;
}
static inline cond_t* CreateCond(void) {
	cond_t* cond = calloc(1, sizeof(cond_t));
	pthread_cond_init(cond, NULL);
	return cond;
}
static inline thread_t* CreateThread(void *func, const char* name, void *data) {
	thread_t* thread = calloc(1, sizeof(thread_t));
	pthread_create(thread, NULL, func, data);
	return thread;
}
#else
#include <SDL2/SDL_thread.h>
#define thread_t SDL_Thread
#define mutex_t SDL_mutex
#define cond_t SDL_cond
#define LockMutex SDL_LockMutex
#define UnlockMutex SDL_UnlockMutex
#define CondSignal SDL_CondSignal
#define CondBroadcast SDL_CondBroadcast
#define CondWait SDL_CondWait
#define CreateMutex SDL_CreateMutex
#define CreateCond SDL_CreateCond
#define CreateThread SDL_CreateThread
#define WaitThread SDL_WaitThread
#define DestroyMutex SDL_DestroyMutex
#define DestroyCond SDL_DestroyCond
#endif

#endif
