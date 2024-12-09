//
// Created by chenrun on 2024/10/29.
//

#include <string.h>
#include <stdio.h>

#include "arc.h"
#include "conf.h"

/****************************************
 * Private Functions
 ****************************************/

/* List Tree Operations */

static inline void arc_list_init(struct arc_list_s *list) {
  list->prev = list;
  list->next = list;
}

static inline void arc_list_insert(struct arc_list_s *head, struct arc_list_s *node) {
  node->next = head->next;
  node->prev = head;
  head->next->prev = node;
  head->next = node;
}

static inline void arc_list_remove(struct arc_list_s *node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->prev = node->next = NULL;
}

static inline void dump_list(struct arc_list_s *list) {
  struct arc_list_s *current = list->next;
  if (current == list) {
    printf("List is empty.\n");
    return;
  }
  while (current != list) {
    struct arc_object_s *obj = arc_container_of(current, struct arc_object_s, list);
    printf("arc_object: %p\n", obj);
    current = current->next;
  }
}

/* Adaptive_replacement_cache Operation */

/****************************************
 * arc_obj_create
 ****************************************/

static struct arc_object_s *arc_obj_create(struct arc_s *cache, const void *key) {
  struct arc_object_s *obj = NULL;

  obj = cache->ops->create(key);
  if (obj) {
    arc_list_init(&obj->list);
  }

  return obj;
}

/****************************************
 * arc_remove_from
 ****************************************/

static void arc_status_init(struct arc_status_s *status, eARCType type) {
  arc_list_init(&status->objects.list);
  status->objects.type = type;
}

/****************************************
 * arc_remove_from
 ****************************************/

static struct arc_object_s *arc_remove_from(struct arc_s *cache, eARCType type) {
  struct arc_object_s *remove = NULL;
  struct arc_list_s *head = NULL;

  switch (type) {
    case MRU:
      head = &cache->mru.objects.list;
      break;
    case MFU:
      head = &cache->mfu.objects.list;
      break;
    case GMRU:
      head = &cache->gmru.objects.list;
      break;
    case GMFU:
      head = &cache->gmfu.objects.list;
      break;
    default:
      return NULL;
  }

  if (head->next != head) {
    remove = arc_container_of(head->prev, struct arc_object_s, list);
    arc_list_remove(&remove->list);
    switch (type) {
      case MRU: cache->mru.size--; break;
      case MFU: cache->mfu.size--; break;
      case GMRU: cache->gmru.size--; break;
      case GMFU: cache->gmfu.size--; break;
      default: break;
    }
  }

  if (remove) {
    remove->list.prev = NULL;
    remove->list.next = NULL;
    remove->type = None;
  }

  return remove;
}

/****************************************
 * arc_insert_to
 ****************************************/

static void arc_insert_to(struct arc_s *cache, struct arc_object_s *obj, eARCType type) {
  switch (obj->type) {
    case MRU: cache->mru.size--; break;
    case MFU: cache->mfu.size--; break;
    case GMRU: cache->gmru.size--; break;
    case GMFU: cache->gmfu.size--; break;
    default:break;
  }

  switch (type) {
    case MRU:
      arc_list_insert(&cache->mru.objects.list, &obj->list);
      cache->mru.size++;
      break;

    case MFU:
      arc_list_insert(&cache->mfu.objects.list, &obj->list);
      cache->mfu.size++;
      break;

    case GMRU:
      arc_list_insert(&cache->gmru.objects.list, &obj->list);
      cache->gmru.size++;
      break;

    case GMFU:
      arc_list_insert(&cache->gmfu.objects.list, &obj->list);
      cache->gmfu.size++;
      break;

    default:
      break;
  }

  obj->type = type;
}

/****************************************
 * arc_update
 ****************************************/

static void arc_update(struct arc_s *cache, struct arc_object_s *obj, eARCType type) {
  if (obj->type != None) {
    /* Let's take that element out of the original chain first */
    arc_list_remove(&obj->list);
  }

  if (type == None) {
    /* This object is going to be removed from the cache */
    cache->ops->destroy(obj);
    return;
  } else if (type == GMFU || type == GMRU) {
    /* The element will be moved to the Ghost List, so it will be ready to be evicted from the Cache */
    cache->ops->evacuate(obj);
  } else if (obj->type != MRU && obj->type != MFU) {
    /* This is the case where we want to move from the Ghost list back to the normal cache list */
    while (cache->mru.size + cache->mfu.size >= cache->ce) {
      struct arc_object_s *removed = NULL;
      if (cache->mru.size > cache->p) {
        /* Remove an object from MRU and move it to the Ghost List */
        removed = arc_remove_from(cache, MRU);
        if (removed) {
          arc_update(cache, removed, GMRU);
        }
      } else if (cache->mfu.size >= 0) {
        /* Remove an object from MFU and move it to the Ghost List */
        removed = arc_remove_from(cache, MFU);
        if (removed) {
          arc_update(cache, removed, GMFU);
        }
      } else {
        break;
      }
    }

    /* Balance Ghost List */
    while (cache->gmfu.size + cache->gmru.size > cache->ce) {
      struct arc_object_s *removed = NULL;
      if (cache->gmru.size > cache->p) {
        removed = arc_remove_from(cache, GMRU);
        arc_update(cache, removed, None);
      } else if (cache->gmfu.size > cache->p) {
        removed = arc_remove_from(cache, GMFU);
        arc_update(cache, removed, None);
      } else {
        break;
      }
    }

    /* Fetch the object when it back to the cache list */
    if (cache->ops->fetch(obj) != 0) {
      /* It's fetch failed, put the cache back to the origin list */
      arc_insert_to(cache, obj, obj->type);
      return;
    }
  }

  /* Move to the head of the specified type list */
  arc_insert_to(cache, obj, type);
}

/****************************************
 * arc_search
 ****************************************/

struct arc_object_s *arc_search(struct arc_s *cache, const void *key) {
  struct arc_list_s *list_node;

  for (list_node = cache->mru.objects.list.next; list_node != &cache->mru.objects.list; list_node = list_node->next) {
    struct arc_object_s *obj = arc_container_of(list_node, struct arc_object_s, list);
    if (cache->ops->cmp(key, obj) == 0) {
      return obj;
    }
  }

  for (list_node = cache->mfu.objects.list.next; list_node != &cache->mfu.objects.list; list_node = list_node->next) {
    struct arc_object_s *obj = arc_container_of(list_node, struct arc_object_s, list);
    if (cache->ops->cmp(key, obj) == 0) {
      return obj;
    }
  }

  for (list_node = cache->gmru.objects.list.next; list_node != &cache->gmru.objects.list; list_node = list_node->next) {
    struct arc_object_s *obj = arc_container_of(list_node, struct arc_object_s, list);
    if (cache->ops->cmp(key, obj) == 0) {
      return obj;
    }
  }

  for (list_node = cache->gmfu.objects.list.next; list_node != &cache->gmfu.objects.list; list_node = list_node->next) {
    struct arc_object_s *obj = arc_container_of(list_node, struct arc_object_s, list);
    if (cache->ops->cmp(key, obj) == 0) {
      return obj;
    }
  }

  return NULL;
}


/****************************************
 * Public Functions
 ****************************************/

/****************************************
 * arc_lookup
 ****************************************/

struct arc_object_s *arc_lookup(struct arc_s *cache, const void *key) {
  struct arc_object_s *obj = NULL;

  obj = arc_search(cache, key);
  if (!obj) {
    obj = arc_obj_create(cache, key);
    if (!obj) {
      return NULL;
    }

    arc_update(cache, obj, MRU);
    return obj;
  } else {
    if (obj->type == GMFU) {
      cache->p = ARC_MAX(0, cache->p - ARC_MAX(cache->gmru.size / cache->gmfu.size, 1));
    } else if (obj->type == GMRU) {
      cache->p = ARC_MIN(cache->ce, cache->p + ARC_MAX(cache->gmfu.size / cache->gmru.size, 1));
    } else {
      arc_assert(false);
    }
  }

  arc_update(cache, obj, MFU);
  return obj;
}

/****************************************
 * arc_destroy
 ****************************************/

void arc_destroy(struct arc_s *cache) {
  struct arc_object_s *obj;

  while ((obj = arc_remove_from(cache, MRU)) != NULL) {
    arc_update(cache, obj, None);
  }
  while ((obj = arc_remove_from(cache, MFU)) != NULL) {
    arc_update(cache, obj, None);
  }

  while ((obj = arc_remove_from(cache, GMRU)) != NULL) {
    arc_update(cache, obj, None);
  }
  while ((obj = arc_remove_from(cache, GMFU)) != NULL) {
    arc_update(cache, obj, None);
  }

  arc_free(cache);
}

/****************************************
 * arc_create
 ****************************************/

struct arc_s *arc_create(struct arc_ops_s *ops, unsigned int cache_size) {
  struct arc_s *cache;

  cache = arc_malloc(sizeof(struct arc_s));
  memset(cache, 0, sizeof(struct arc_s));
  arc_assert(cache);

  arc_status_init(&cache->mru, MRU);
  arc_status_init(&cache->mfu, MFU);
  arc_status_init(&cache->gmru, GMRU);
  arc_status_init(&cache->gmfu, GMFU);

  cache->ce = cache_size;
  cache->p = cache->ce >> 1;
  cache->ops = ops;
  return cache;
}

/****************************************
 * arc_dump
 ****************************************/

void arc_dump(struct arc_s *arc) {
  printf("MRU List size[%d]:\n",arc->mru.size);
  dump_list(&arc->mru.objects.list);

  printf("MFU List size[%d]:\n",arc->mfu.size);
  dump_list(&arc->mfu.objects.list);

  printf("GMFU List size[%d]:\n",arc->gmfu.size);
  dump_list(&arc->gmfu.objects.list);

  printf("GMRU List:[%d]:\n",arc->gmru.size);
  dump_list(&arc->gmru.objects.list);
}