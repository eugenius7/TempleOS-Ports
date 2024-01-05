
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "load.h"


size_t init_load (const char * source_fname)
{
    uint64_t end_address;
    FILE * fd;
    
    fd = fopen (source_fname, "rb");
    if (fd == NULL) {
        perror ("unable to open input file");
        exit (1);
    }
    fseek (fd, 4 * 8, SEEK_SET);
    if (fread (&end_address, 8, 1, fd) != 1) {
        perror ("unable to read input file header");
        exit (1);
    }
    if (end_address > MAX_SIZE) {
        fputs ("end_address invalid", stderr);
        exit (1);
    }
    fclose (fd);
    return (size_t) end_address;
}

void do_load (uint64_t * destination, void * syscall_handler,
              const char * source_fname)
{
    uint64_t load_size, total_size, i;
    uint32_t reloc;
    FILE * fd;

    fd = fopen (source_fname, "rb");
    if (fd == NULL) {
        perror ("unable to open input file");
        exit (1);
    }
    fseek (fd, 5 * 8, SEEK_SET);
    if ((fread (&load_size, 8, 1, fd) != 1)
    || (fread (&total_size, 8, 1, fd) != 1)) {
        perror ("unable to read input file header");
        exit (1);
    }
    if ((load_size > total_size) || (total_size > MAX_SIZE)) {
        fputs ("load_size/total_size invalid", stderr);
        exit (1);
    }
    /* read the data */
    fseek (fd, 0, SEEK_SET);
    if (fread (destination, load_size, 1, fd) != 1) {
        perror ("unable to read input payload");
        exit (1);
    }
    /* read the relocation table.. */
    for (i = load_size; i < total_size; i += 4) {
        if (fread (&reloc, 4, 1, fd) != 1) {
            perror ("unable to read relocation table");
            exit (1);
        }
        if (reloc >= (load_size / 8)) {
            fputs ("relocation invalid", stderr);
            exit (1);
        }
        destination[reloc] += (uint64_t) destination;
    }
    assert (ftell (fd) == total_size);
    destination[2] = (uint64_t) syscall_handler;
    fclose (fd);
}



