#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "load.h"

#define MAX_FD_HANDLES 0x10

#define O_WRONLY         01
#define O_RDWR           02

#define    STDIN_FILENO    0   /* Standard input.  */
#define    STDOUT_FILENO   1   /* Standard output.  */
#define    STDERR_FILENO   2   /* Standard error output */

#define __KERNEL_NCCS 19
typedef unsigned int    tcflag_t;
typedef unsigned char   cc_t;

struct __kernel_termios
{
    tcflag_t c_iflag;  /* input mode flags */
    tcflag_t c_oflag;  /* output mode flags */
    tcflag_t c_cflag;  /* control mode flags */
    tcflag_t c_lflag;  /* local mode flags */
    cc_t c_line;  /* line discipline */
    cc_t c_cc[__KERNEL_NCCS]; /* control characters */
};


static uint64_t break_start, break_end;
static FILE * fd_handle[MAX_FD_HANDLES];

void init_syscall (uint64_t _break_start, uint64_t _break_end)
{
    fd_handle[STDIN_FILENO] = stdin;
    fd_handle[STDOUT_FILENO] = stdout;
    fd_handle[STDERR_FILENO] = stderr;
    break_start = _break_start;
    break_end = _break_end;
}

static int64_t do_ioctl (int64_t p0, int64_t p1, int64_t p2)
{
    struct __kernel_termios * k;
    switch (p1) {
        case 0x5401:
            /* TCGETS */
            k = (struct __kernel_termios *) p2;
            memset (k, 0, sizeof (struct __kernel_termios));
            return 0;
        default:
            printf ("Unsupported ioctl %u p1 %p p2 %p\n",
                    (unsigned) p0, (void *) p1, (void *) p2);
            exit (1);
            return 0;
    }
}

static int64_t do_open (int64_t p0, int64_t p1)
{
    unsigned i;
    FILE * fd;

    if ((p1 & 3) == 0) {
        fd = fopen ((const char *) p0, "rb");
    } else if (p1 == 0x241) {
        fd = fopen ((const char *) p0, "wb");
    } else {
        printf ("Unsupported open mode for '%s': 0x%x\n",
                (const char *) p0, (unsigned) p1);
        exit (1);
    }
    if (fd == NULL) {
        /* file not found, perhaps? */
        return -1;
    }
    for (i = 0; i < MAX_FD_HANDLES; i++) {
        if (fd_handle[i] == NULL) {
            fd_handle[i] = fd;
            fd = NULL;
            printf ("open '%s' -> fd %u\n", (const char *) p0, i);
            return i;
        }
    }
    printf ("No handles available for '%s'\n",
            (const char *) p0);
    exit (1);
    return -1;
}

int64_t syscall_handler (int64_t syscall_number,
        int64_t p0, int64_t p1, int64_t p2)
{
    int64_t rc;

    switch (syscall_number) {
        case 0x10:
            return do_ioctl (p0, p1, p2);
        case 2:
            return do_open (p0, p1);
        case 0:
        case 1:
        case 3:
        case 5:
        case 8:
            if ((p0 < 0) || (p0 >= (int64_t) MAX_FD_HANDLES)
            || (fd_handle[p0] == NULL)) {
                printf ("Invalid handle: 0x%u\n", (unsigned) p0);
                exit (1);
            }
            switch (syscall_number) {
                case 0:
                    fflush (stdout);
                    if ((p0 == STDIN_FILENO) && (p2 > 0)) {
                        char * s = (char *) p1;
                        fgets (s, p2, stdin);
                        return strlen (s);
                    }
                    rc = fread ((void *) p1, 1, p2, fd_handle[p0]);
                    if (p0 != STDIN_FILENO) {
                        /* printf ("read %u bytes from fd %u to %p\n",
                                (unsigned) rc, (unsigned) p0,
                                (void *) p1); */
                    }
                    break;
                case 1:
                    rc = fwrite ((void *) p1, 1, p2, fd_handle[p0]);
                    fflush (fd_handle[p0]);
                    break;
                case 5:
                    {
                        long save;
                        memset ((void *) p1, 0, 144);

                        save = ftell (fd_handle[p0]);
                        fseek (fd_handle[p0], 0, SEEK_END);
                        ((uint64_t *) p1)[6] = ftell (fd_handle[p0]);
                        fseek (fd_handle[p0], save, SEEK_SET);
                    }
                    return 0;
                case 8:
                    fseek (fd_handle[p0], p1, p2);
                    return ftell (fd_handle[p0]);
                default:
                    fclose (fd_handle[p0]);
                    fd_handle[p0] = NULL;
                    rc = 0;
                    break;
            }
            return rc;
        case 60:
            exit (p0);
            return 1;
        case 12:
            if (p0 < break_start) {
                p0 = break_start;
            }
            if (p0 > break_end) {
                p0 = break_end;
            }
            printf ("brk %p, %u bytes\n",
                    (void *) p0, (unsigned) (p0 - break_start));
            return p0;
        case 201:
            /* time */
            return 1400000000;
        case 96:
            /* gettimeofday */
            if (p0) {
                memset ((void *) p0, 0, 16);
                ((uint64_t *) p0)[0] = 1400000000;
            }
            if (p1) {
                memset ((void *) p1, 0, 8);
            }
            return 0;
        default:
            printf ("Unsupported system call %u p0 %p p1 %p p2 %p\n",
                    (unsigned) syscall_number, (void *) p0, (void *) p1, (void *) p2);
            exit (1);
            return 0;
    }
}


