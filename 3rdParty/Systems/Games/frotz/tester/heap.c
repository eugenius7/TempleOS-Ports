#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>



int main (int argc, char ** argv)
{
    unsigned *  heap_test;
    unsigned    heap_size, i, j;

    if (argc != 2) {
        puts ("give the heap size\n");
        return 1;
    }
    heap_size = atoi (argv[1]);

    heap_test = malloc (heap_size);
    if (heap_test == NULL) {
        puts ("heap_test is NULL\n");
        return 1;
    }
    for (i = 0; i < (heap_size / 4); i++) {
        heap_test[i] = (unsigned) ((intptr_t) &heap_test[i]);
    }
    for (j = 0; j < 1000000000; ) {
        for (i = 0; i < (heap_size / 4); i++) {
            if (heap_test[i] != (unsigned) ((intptr_t) &heap_test[i])) {
                puts ("heap_test changed\n");
                return 1;
            }
        }
        j += i;
    }
    free (heap_test);
    puts ("success, no changes\n");
    return 0;
}

