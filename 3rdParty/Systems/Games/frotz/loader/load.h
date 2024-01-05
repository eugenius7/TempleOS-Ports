#ifndef LOAD_H
#define LOAD_H

#include <stdint.h>

#define MAX_HEAP_SIZE 0x1000000     /* 16M - heap size */
#define MAX_SIZE      0x1000000     /* 16M - program size */
#define MAX_STACK_SIZE 0x20000      /* 128k - stack size */

int64_t syscall_handler (int64_t syscall_number,
        int64_t p0, int64_t p1, int64_t p2);
void init_syscall (uint64_t _break_start, uint64_t _break_end);

size_t init_load (const char * source_fname);
void do_load (uint64_t * destination, void * syscall_handler,
              const char * source_fname);

#endif

