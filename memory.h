#ifndef MEMORY_H
#define MEMORY_H

#include "base.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define mymalloc malloc
#define mymemset memset
#define myfree free
#define myrealloc realloc
#define mymemcpy memcpy
#define mymemmove memmove
#define checkPointer(...)
#define checkMemory(...)

#ifdef __cplusplus
}
#endif

#endif

