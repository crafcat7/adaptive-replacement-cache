#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../arc.h"

#define MAX_KEYS 20
#define KEY_LENGTH 20

#define DEMO_OBJECT_FROM_ARC(obj) \
  ((struct demo_object_s *)((char *)(obj) - offsetof(struct demo_object_s, arc_obj)))

struct demo_object_s {
  struct arc_object_s arc_obj;
  char *data;
  char *key;
};

int demo_cmp(const void *key, struct arc_object_s *obj) {
  struct demo_object_s *demo_obj = DEMO_OBJECT_FROM_ARC(obj);
  return strcmp((const char *)demo_obj->key, (const char *)key);
}

int demo_fetch(struct arc_object_s *obj) {
  struct demo_object_s *demo_obj = DEMO_OBJECT_FROM_ARC(obj);
  demo_obj->data = strdup("fetch");
  return 0;
}

struct arc_object_s *demo_create(const void *key) {
  struct demo_object_s *demo_obj = malloc(sizeof(struct demo_object_s));
  if (demo_obj) {
    demo_obj->key = strdup((const char *)key);
    demo_obj->data = strdup("create");
    printf("create demo_obj:%p create, key:%s, data:%s\n", demo_obj, demo_obj->key, demo_obj->data);
  }
  return &demo_obj->arc_obj;
}

void demo_evacuate(struct arc_object_s *obj) {
  struct demo_object_s *demo_obj = DEMO_OBJECT_FROM_ARC(obj);
//  printf("evacuate demo_obj:%p, key:%s\n", demo_obj, demo_obj->key);
  free(demo_obj->data);
  demo_obj->data = strdup("evacuated");
  if (!demo_obj->data) {
    fprintf(stderr, "Failed to allocate memory for data.\n");
  }
}

void demo_destroy(struct arc_object_s *obj) {
  struct demo_object_s *demo_obj = DEMO_OBJECT_FROM_ARC(obj);
  free(demo_obj->key);
  free(demo_obj->data);
  free(demo_obj);
}

struct arc_ops_s demo_arc_ops = {
        .cmp = demo_cmp,
        .fetch = demo_fetch,
        .create = demo_create,
        .evacuate = demo_evacuate,
        .destroy = demo_destroy,
};

int main() {
  struct arc_object_s *found_obj;
  struct arc_s *cache;
  unsigned int cache_size = 10;

  cache = arc_create(&demo_arc_ops, cache_size);
  if (!cache) {
    fprintf(stderr, "Memory alloc failed\n");
    return EXIT_FAILURE;
  }

  for (int i = 0; i <= MAX_KEYS; i++) {
    struct arc_object_s *testobj = NULL;
    char key[KEY_LENGTH];
    snprintf(key, sizeof(key), "example_key_%d", i);
    testobj = arc_lookup(cache, key);
    arc_dump(cache);
    found_obj = arc_lookup(cache, "example_key_0");
    found_obj = arc_lookup(cache, "example_key_1");
    printf("=======================================\n");
  }
  arc_destroy(cache);
  return 0;
}