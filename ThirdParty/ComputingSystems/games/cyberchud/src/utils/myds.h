#ifndef UTILS_MYDS_H
#define UTILS_MYDS_H

#include "utils/mymalloc.h"

#define STBDS_REALLOC(c,p,s) myrealloc(p,s)
#define STBDS_FREE(c,p) myfree(p)
#include "stb_ds.h"

#endif
