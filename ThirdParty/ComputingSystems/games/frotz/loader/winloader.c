#include <stdio.h>
#include <windows.h>
#include <assert.h>

#include "load.h"

void go_shim (uint64_t * p1, int argc, char ** argv);
void syscall_shim (void);

int main (int argc, char ** argv)
{
    uint64_t *      p0;
    const char *    source_fname;
    size_t          embed_minimum_size;

    if (argc < 2) {
        fputs ("Usage: winloader.exe <program.prg> [args...]\n", stderr);
        return 1;
    }

    source_fname = argv[1];

    embed_minimum_size = init_load (source_fname);
    p0 = VirtualAlloc (NULL, embed_minimum_size + MAX_HEAP_SIZE,
                 MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    assert (p0 != NULL);
    printf ("p0 = %p\n", p0);

    init_syscall
        ((((uint64_t) p0) + embed_minimum_size),
         (((uint64_t) p0) + embed_minimum_size + MAX_HEAP_SIZE));
    do_load (p0, syscall_shim, source_fname);

    printf ("launch = %p\n\n\n\n", p0);

    go_shim (p0, argc - 1, &argv[1]);
    /* failure...! */
    return 1;
}

