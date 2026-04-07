//
// Adaptive Replacement Cache (ARC) - Single Header Library
//

#ifndef ADAPTIVE_REPLACEMENT_CACHE_ARC_H
#define ADAPTIVE_REPLACEMENT_CACHE_ARC_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/****************************************
 * Configuration Macros
 ****************************************/

#define arc_malloc(size) malloc(size)
#define arc_free(ptr) free(ptr)
#define arc_assert(cond) do { if (!(cond)) { fprintf(stderr, "ASSERTION FAILED: " #cond "\n"); } } while(0)
#define ARC_MAX(a, b) ((a) > (b) ? (a) : (b))
#define ARC_MIN(a, b) ((a) < (b) ? (a) : (b))

#define arc_container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/****************************************
 * List Elements
 ****************************************/

struct arc_list_s {
  struct arc_list_s *prev;
  struct arc_list_s *next;
};

/****************************************
 * ARC Type Definitions
 ****************************************/

typedef enum {
  NONE,
  T1,
  T2,
  B1,
  B2,
} eARCType;

struct arc_object_s {
  struct arc_list_s list;
  eARCType type;
  void *key;
  void *value;
};

struct arc_status_s {
  struct arc_object_s objects;
  unsigned int size;
};

struct arc_ops_s {
  int (*cmp)(const void *key, const void *other_key);
  unsigned int (*hash)(const void *key, unsigned int hash_size);
  void *(*fetch)(const void *key, struct arc_object_s *obj);
  struct arc_object_s *(*create)(const void *key, void *value);
  void (*evacuate)(struct arc_object_s *obj);
  void (*destroy)(struct arc_object_s *obj);
  void *(*duplicate_key)(const void *key);
  void (*free_key)(void *key);
  void (*free_value)(void *value);
};

typedef struct arc_node_s {
  struct arc_object_s *obj;
  struct arc_node_s *next;
} arc_node_t;

struct arc_s {
  struct arc_ops_s *ops;
  struct arc_status_s t1;
  struct arc_status_s t2;
  struct arc_status_s b1;
  struct arc_status_s b2;
  unsigned int c;
  unsigned int p;
  unsigned int hash_size;
  arc_node_t **hash_table;
};

/****************************************
 * Private List Operations
 ****************************************/

static inline void arc_list_init(struct arc_list_s *list) {
  list->prev = list;
  list->next = list;
}

static inline void arc_list_insert(struct arc_list_s *head, struct arc_list_s *node) {
  if (!head || !node || node == head) return;
  node->next = head->next;
  node->prev = head;
  head->next->prev = node;
  head->next = node;
}

static inline void arc_list_remove(struct arc_list_s *node) {
  if (!node || !node->prev || !node->next) return;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->prev = node->next = NULL;
}

/****************************************
 * Hash Table Operations
 ****************************************/

static inline unsigned int arc_hash_default(const void *key, unsigned int hash_size) {
  unsigned int h = 0;
  if (!key) return 0;
  const unsigned char *s = (const unsigned char *)key;
  while (*s) { h = h * 31 + *s++; }
  return h % hash_size;
}

static inline unsigned int arc_hash_key(struct arc_s *cache, const void *key) {
  if (!cache || !key) return 0;
  if (cache->ops && cache->ops->hash) return cache->ops->hash(key, cache->hash_size);
  return arc_hash_default(key, cache->hash_size);
}

static inline void arc_hash_insert(struct arc_s *cache, struct arc_object_s *obj) {
  if (!cache || !cache->hash_table || !obj || !obj->key) {
    return;
  }
  unsigned int h = arc_hash_key(cache, obj->key);
  arc_node_t *node = cache->hash_table[h];
  while (node) {
    if (node->obj && cache->ops->cmp(obj->key, node->obj->key) == 0) {
      node->obj = obj;
      return;
    }
    node = node->next;
  }
  node = (arc_node_t *)arc_malloc(sizeof(arc_node_t));
  if (node) {
    node->obj = obj;
    node->next = cache->hash_table[h];
    cache->hash_table[h] = node;
  }
}

static inline void arc_hash_remove(struct arc_s *cache, struct arc_object_s *obj) {
  if (!cache || !cache->hash_table || !obj || !obj->key) return;
  unsigned int h = arc_hash_key(cache, obj->key);
  arc_node_t **prev = &cache->hash_table[h];
  arc_node_t *node = cache->hash_table[h];
  while (node) {
    if (node->obj == obj) {
      *prev = node->next;
      arc_free(node);
      return;
    }
    prev = &node->next;
    node = node->next;
  }
}

static inline struct arc_object_s *arc_hash_lookup(struct arc_s *cache, const void *key) {
  if (!cache || !key || !cache->ops || !cache->hash_table) return NULL;
  unsigned int h = arc_hash_key(cache, key);
  arc_node_t *node = cache->hash_table[h];
  while (node) {
    if (node->obj && node->obj->key && cache->ops->cmp(key, node->obj->key) == 0) {
      return node->obj;
    }
    node = node->next;
  }
  return NULL;
}

static inline void arc_hash_destroy(struct arc_s *cache) {
  if (!cache || !cache->hash_table) return;
  for (unsigned int i = 0; i < cache->hash_size; i++) {
    arc_node_t *node = cache->hash_table[i];
    while (node) { arc_node_t *next = node->next; arc_free(node); node = next; }
  }
  arc_free(cache->hash_table);
  cache->hash_table = NULL;
}

/****************************************
 * Private Helper Functions
 ****************************************/

static inline void arc_status_init(struct arc_status_s *status, eARCType type) {
  arc_list_init(&status->objects.list);
  status->objects.type = type;
}

static inline void arc_status_inc(struct arc_s *cache, eARCType type) {
  switch (type) {
    case T1: cache->t1.size++; break;
    case T2: cache->t2.size++; break;
    case B1: cache->b1.size++; break;
    case B2: cache->b2.size++; break;
    default: break;
  }
}

static inline void arc_status_dec(struct arc_s *cache, eARCType type) {
  switch (type) {
    case T1: if (cache->t1.size > 0) cache->t1.size--; break;
    case T2: if (cache->t2.size > 0) cache->t2.size--; break;
    case B1: if (cache->b1.size > 0) cache->b1.size--; break;
    case B2: if (cache->b2.size > 0) cache->b2.size--; break;
    default: break;
  }
}

static inline struct arc_object_s *arc_remove_from(struct arc_s *cache, eARCType type, eARCType new_type) {
  if (!cache) return NULL;
  struct arc_list_s *head = NULL;
  switch (type) {
    case T1: head = &cache->t1.objects.list; break;
    case T2: head = &cache->t2.objects.list; break;
    case B1: head = &cache->b1.objects.list; break;
    case B2: head = &cache->b2.objects.list; break;
    default: return NULL;
  }
  
  if (!head || !head->next || !head->prev) return NULL;
  
  struct arc_list_s *list_ptr = head->prev;
  if (!list_ptr || list_ptr == head) return NULL;
  
  struct arc_object_s *remove = arc_container_of(list_ptr, struct arc_object_s, list);
  
  if (remove->list.prev == NULL && remove->list.next == NULL) {
    return NULL;
  }
  
  arc_list_remove(&remove->list);
  arc_hash_remove(cache, remove);
  arc_status_dec(cache, type);
  remove->type = new_type;
  return remove;
}

static inline void arc_insert_to(struct arc_s *cache, struct arc_object_s *obj, eARCType type) {
  if (!obj || !cache) return;
  
  eARCType old_type = obj->type;
  if (old_type != NONE) {
    arc_status_dec(cache, old_type);
    arc_list_remove(&obj->list);
  }
  
  struct arc_list_s *head = NULL;
  switch (type) {
    case T1: head = &cache->t1.objects.list; break;
    case T2: head = &cache->t2.objects.list; break;
    case B1: head = &cache->b1.objects.list; break;
    case B2: head = &cache->b2.objects.list; break;
    default: return;
  }
  
  if (!head || !head->next || !head->prev) return;
  
  arc_list_insert(head, &obj->list);
  arc_status_inc(cache, type);
  
  obj->type = type;
  arc_hash_insert(cache, obj);
}

static inline struct arc_object_s *arc_obj_create(struct arc_s *cache, const void *key, void *value) {
  if (!cache || !cache->ops) return NULL;
  struct arc_object_s *obj = cache->ops->create(key, value);
  if (obj) {
    arc_list_init(&obj->list);
    if (obj->type != NONE) obj->type = NONE;
  }
  return obj;
}

static inline void arc_obj_destroy(struct arc_s *cache, struct arc_object_s *obj) {
  if (!cache || !obj) return;
  obj->list.prev = obj->list.next = NULL;
  obj->type = NONE;
  if (cache->ops && cache->ops->destroy) {
    cache->ops->destroy(obj);
  }
}

/****************************************
 * ARC Core Operations
 ****************************************/

static inline void arc_replace(struct arc_s *cache, eARCType target_ghost) {
  if (!cache) return;
  unsigned int t1 = cache->t1.size;
  unsigned int t2 = cache->t2.size;
  unsigned int p = cache->p;
  
  if (t1 == 0 && t2 == 0) return;
  struct arc_object_s *removed = NULL;
  
  if (t1 > 0 && ((target_ghost == B2 && t1 == p) || (t1 > p))) {
    removed = arc_remove_from(cache, T1, NONE);
    if (removed) {
      cache->ops->evacuate(removed);
      arc_insert_to(cache, removed, B1);
      return;
    }
  }

  if (t2 > 0) {
    removed = arc_remove_from(cache, T2, NONE);
    if (removed) {
      cache->ops->evacuate(removed);
      arc_insert_to(cache, removed, B2);
      return;
    }
  }

  if (t1 > 0) {
    removed = arc_remove_from(cache, T1, NONE);
    if (removed) {
      cache->ops->evacuate(removed);
      arc_insert_to(cache, removed, B1);
    }
  }
}

static inline void arc_maintain_constraints(struct arc_s *cache) {
  if (!cache) return;
  while (cache->b1.size > cache->c) {
    struct arc_object_s *r = arc_remove_from(cache, B1, NONE);
    if (r) {
      arc_obj_destroy(cache, r);
    } else {
      fprintf(stderr, "ERROR: maintain_constraints: b1.size=%u > c=%u but arc_remove_from returned NULL\n", 
              cache->b1.size, cache->c);
      break;
    }
  }
  while (cache->b2.size > cache->c) {
    struct arc_object_s *r = arc_remove_from(cache, B2, NONE);
    if (r) {
      arc_obj_destroy(cache, r);
    } else {
      fprintf(stderr, "ERROR: maintain_constraints: b2.size=%u > c=%u but arc_remove_from returned NULL\n", 
              cache->b2.size, cache->c);
      break;
    }
  }

  while ((cache->b1.size + cache->b2.size) > cache->c) {
    struct arc_object_s *r = NULL;
    if (cache->b1.size >= cache->b2.size) {
      r = arc_remove_from(cache, B1, NONE);
    } else {
      r = arc_remove_from(cache, B2, NONE);
    }
    if (!r) break;
    arc_obj_destroy(cache, r);
  }
}

/****************************************
 * Public API
 ****************************************/

static inline struct arc_s *arc_create(struct arc_ops_s *ops, unsigned int cache_size) {
  struct arc_s *cache = (struct arc_s *)arc_malloc(sizeof(struct arc_s));
  if (!cache) return NULL;
  memset(cache, 0, sizeof(struct arc_s));
  
  arc_status_init(&cache->t1, T1);
  arc_status_init(&cache->t2, T2);
  arc_status_init(&cache->b1, B1);
  arc_status_init(&cache->b2, B2);
  
  cache->c = cache_size > 0 ? cache_size : 1;
  cache->p = cache->c >> 1;
  cache->ops = ops;
  cache->hash_size = cache->c * 8;
  if (cache->hash_size < 16) cache->hash_size = 16;
  
  cache->hash_table = (arc_node_t **)arc_malloc(sizeof(arc_node_t *) * cache->hash_size);
  if (!cache->hash_table) { arc_free(cache); return NULL; }
  memset(cache->hash_table, 0, sizeof(arc_node_t *) * cache->hash_size);
  return cache;
}

static inline struct arc_object_s *arc_lookup(struct arc_s *cache, const void *key) {
  if (!cache || !key || !cache->ops) return NULL;
  
  struct arc_object_s *obj = arc_hash_lookup(cache, key);

  if (obj) {
    if (obj->type == T2) {
      arc_insert_to(cache, obj, T2);
    } else if (obj->type == T1) {
      arc_insert_to(cache, obj, T2);
    } else if (obj->type == B1) {
      unsigned int delta = ARC_MAX(1U, cache->b2.size / ARC_MAX(1U, cache->b1.size));
      cache->p = ARC_MIN(cache->c, cache->p + delta);

      if (cache->t1.size + cache->t2.size >= cache->c) {
        arc_replace(cache, B1);
      }

      obj->value = cache->ops->fetch(obj->key, obj);
      if (!obj->value) {
        obj = arc_remove_from(cache, B1, NONE);
        if (obj) arc_obj_destroy(cache, obj);
        return NULL;
      }

      arc_insert_to(cache, obj, T2);
      arc_maintain_constraints(cache);
    } else if (obj->type == B2) {
      unsigned int delta = ARC_MAX(1U, cache->b1.size / ARC_MAX(1U, cache->b2.size));
      cache->p = (cache->p > delta) ? (cache->p - delta) : 0;

      if (cache->t1.size + cache->t2.size >= cache->c) {
        arc_replace(cache, B2);
      }

      obj->value = cache->ops->fetch(obj->key, obj);
      if (!obj->value) {
        obj = arc_remove_from(cache, B2, NONE);
        if (obj) arc_obj_destroy(cache, obj);
        return NULL;
      }

      arc_insert_to(cache, obj, T2);
      arc_maintain_constraints(cache);
    }
    return obj;
  }
  
  obj = arc_obj_create(cache, key, NULL);
  if (!obj) {
    fprintf(stderr, "ERROR: arc_lookup: arc_obj_create returned NULL for key=%s\n", (char*)key);
    return NULL;
  }
  
  obj->value = cache->ops->fetch(obj->key, obj);
  if (!obj->value) { arc_obj_destroy(cache, obj); return NULL; }
  
  if (cache->t1.size + cache->t2.size >= cache->c) {
    arc_replace(cache, NONE);
  }
  
  arc_insert_to(cache, obj, T1);

  arc_maintain_constraints(cache);
  
  return obj;
}

static inline unsigned int arc_size(struct arc_s *cache) {
  return cache ? cache->t1.size + cache->t2.size : 0;
}

static inline void arc_destroy(struct arc_s *cache) {
  if (!cache) return;
  
  if (cache->ops && cache->ops->destroy) {
    struct arc_list_s *lists[] = {
      &cache->t1.objects.list,
      &cache->t2.objects.list,
      &cache->b1.objects.list,
      &cache->b2.objects.list
    };
    
    for (int l = 0; l < 4; l++) {
      struct arc_list_s *head = lists[l];
      if (!head || !head->next || !head->prev) continue;
      
      while (head->next != head) {
        struct arc_object_s *obj = arc_container_of(head->next, struct arc_object_s, list);
        arc_list_remove(&obj->list);
        obj->type = NONE;
        cache->ops->destroy(obj);
      }
    }
  }

  cache->t1.size = 0;
  cache->t2.size = 0;
  cache->b1.size = 0;
  cache->b2.size = 0;
  
  if (cache->hash_table) {
    for (unsigned int i = 0; i < cache->hash_size; i++) {
      arc_node_t *node = cache->hash_table[i];
      while (node) { arc_node_t *next = node->next; arc_free(node); node = next; }
    }
    arc_free(cache->hash_table);
    cache->hash_table = NULL;
  }
  
  arc_free(cache);
}

static inline void arc_dump(struct arc_s *arc) {
  if (!arc || !arc->ops) return;
  printf("T1 size=%d, T2 size=%d, B1 size=%d, B2 size=%d, p=%d\n",
         arc->t1.size, arc->t2.size, arc->b1.size, arc->b2.size, arc->p);
}

#endif //ADAPTIVE_REPLACEMENT_CACHE_ARC_H
