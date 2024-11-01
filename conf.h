//
// Created by chenrun on 2024/10/30.
//

#ifndef ADAPTIVE_REPLACEMENT_CACHE_CONF_H
#define ADAPTIVE_REPLACEMENT_CACHE_CONF_H

#include <stdlib.h>
#include <stddef.h>

# define arc_malloc(size) malloc(size)
# define arc_free(ptr) free(ptr)
# define arc_assert(cond)
# define ARC_MAX(a, b) ((a) > (b) ? (a) : (b))
# define ARC_MIN(a, b) ((a) < (b) ? (a) : (b))
#define arc_container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif //ADAPTIVE_REPLACEMENT_CACHE_CONF_H
