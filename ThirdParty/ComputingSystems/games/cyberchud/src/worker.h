#ifndef WORKER_H
#define WORKER_H

#include "engine.h"

int thread_init(engine_threads_t* e);
int thread_set_and_go(engine_threads_t* e, const JOB_STATE state);

#endif
