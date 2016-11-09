#ifndef H_MEMUTIL
#define H_MEMUTIL

#include <stdlib.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define CALLOC_STRUCT(T)   (struct T *) calloc(1, sizeof(struct T))

#endif

