//
// Created by chenrun on 2024/10/29.
//

#ifndef ADAPTIVE_REPLACEMENT_CACHE_ARC_H
#define ADAPTIVE_REPLACEMENT_CACHE_ARC_H

#include <stddef.h>

/* List elements */

struct arc_list_s {
  struct arc_list_s *prev;
  struct arc_list_s *next;
};

/* ARC definitions */

typedef enum {
  None,
  MRU,
  MFU,
  GMRU,
  GMFU,
} eARCType;

struct arc_object_s {
  struct arc_list_s list;
  eARCType type;
};

struct arc_status_s {
  struct arc_object_s objects;
  unsigned int size; /* Track linked list sizes to adjust policies */
};

struct arc_ops_s {
  int (*cmp) (const void *key, struct arc_object_s *obj);
  int (*fetch) (struct arc_object_s *obj);
  struct arc_object_s *(*create) (const void *key);
  void (*evacuate) (struct arc_object_s *obj);
  void (*destroy) (struct arc_object_s *obj);
};

struct arc_s {
  struct arc_ops_s *ops;
  struct arc_status_s mru;
  struct arc_status_s mfu;
  struct arc_status_s gmfu;
  struct arc_status_s gmru;
  unsigned int p;
  unsigned int ce; /* Number of cache entrys */
};

void arc_dump(struct arc_s *arc);
void arc_destroy(struct arc_s *cache);
struct arc_s *arc_create(struct arc_ops_s *ops, unsigned int cache_size);
struct arc_object_s *arc_lookup(struct arc_s *cache, const void *key);

#endif //ADAPTIVE_REPLACEMENT_CACHE_ARC_H
