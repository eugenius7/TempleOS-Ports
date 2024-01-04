#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include "load.h"


void go_shim (uint64_t * p1, int argc, char ** argv)
{
    void (* go) (int argc, char ** argv);
    go = (void *) (p1);
    go (argc, argv);
}

int main (int argc, char ** argv)
{
    uint64_t *      p0;
    const char *    source_fname;
    size_t          embed_minimum_size;

    if (argc < 2) {
        fputs ("Usage: linuxloader.exe <program.prg> [args...]\n", stderr);
        return 1;
    }

    source_fname = argv[1];

    embed_minimum_size = init_load (source_fname);
    p0 = mmap (NULL, embed_minimum_size + MAX_HEAP_SIZE,
              PROT_READ | PROT_WRITE | PROT_EXEC,
              MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    assert (p0 != NULL);
    printf ("p0 = %p\n", p0);

    init_syscall
        ((((uint64_t) p0) + embed_minimum_size),
         (((uint64_t) p0) + embed_minimum_size + MAX_HEAP_SIZE));
    do_load (p0, syscall_handler, source_fname);

    printf ("launch = %p\n\n\n\n", p0);

    go_shim (p0, argc - 1, &argv[1]);
    /* failure...! */
    return 1;
}

