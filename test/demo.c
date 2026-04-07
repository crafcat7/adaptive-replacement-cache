#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../arc.h"

#define DEMO_OBJECT_FROM_ARC(obj) \
  ((struct demo_object_s *)((char *)(obj) - offsetof(struct demo_object_s, arc_obj)))

struct demo_object_s {
  struct arc_object_s arc_obj;
  char *data;
  char *key;
};

static int destroy_count = 0;
static int evacuate_count = 0;
static int fetch_count = 0;

int demo_cmp(const void *key, const void *other_key) {
  return strcmp((const char *)key, (const char *)other_key);
}

void *demo_fetch(const void *key, struct arc_object_s *obj) {
  fetch_count++;
  struct demo_object_s *demo_obj = DEMO_OBJECT_FROM_ARC(obj);
  free(demo_obj->data);
  demo_obj->data = strdup("fetched");
  return demo_obj->data;
}

struct arc_object_s *demo_create(const void *key, void *value) {
  struct demo_object_s *demo_obj = malloc(sizeof(struct demo_object_s));
  if (demo_obj) {
    demo_obj->key = strdup((const char *)key);
    demo_obj->data = strdup("created");
    demo_obj->arc_obj.key = demo_obj->key;
    demo_obj->arc_obj.value = demo_obj->data;
  }
  return &demo_obj->arc_obj;
}

void demo_evacuate(struct arc_object_s *obj) {
  evacuate_count++;
  struct demo_object_s *demo_obj = DEMO_OBJECT_FROM_ARC(obj);
  free(demo_obj->data);
  demo_obj->data = strdup("evacuated");
}

void demo_destroy(struct arc_object_s *obj) {
  destroy_count++;
  struct demo_object_s *demo_obj = DEMO_OBJECT_FROM_ARC(obj);
  free(demo_obj->key);
  free(demo_obj->data);
  free(demo_obj);
}

void *demo_dup_key(const void *key) {
  return strdup((const char *)key);
}

void demo_free_key(void *key) {
  free(key);
}

void demo_free_value(void *value) {
  free(value);
}

char *demo_print_key(const void *key) {
  return (char *)key;
}

static struct arc_ops_s test_ops = {
  .cmp = demo_cmp,
  .hash = NULL,
  .fetch = demo_fetch,
  .create = demo_create,
  .evacuate = demo_evacuate,
  .destroy = demo_destroy,
  .duplicate_key = NULL,
  .free_key = demo_free_key,
  .free_value = demo_free_value,
};

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
  if (cond) { \
    printf("  [PASS] %s\n", msg); \
    tests_passed++; \
  } else { \
    printf("  [FAIL] %s\n", msg); \
    tests_failed++; \
  } \
} while(0)

/***** Basic Tests *****/

static void test_create_destroy(void) {
  printf("\n[Test] Create and destroy\n");
  
  struct arc_s *cache = arc_create(&test_ops, 10);
  TEST_ASSERT(cache != NULL, "arc_create returns non-NULL");
  
  arc_destroy(cache);
  TEST_ASSERT(true, "arc_destroy completes");
}

static void test_null_ops(void) {
  printf("\n[Test] Null ops handling\n");
  
  struct arc_s *cache = arc_create(NULL, 10);
  TEST_ASSERT(cache != NULL, "arc_create with NULL ops returns non-NULL");
  
  if (cache) {
    arc_destroy(cache);
  }
}

static void test_boundary_size(void) {
  printf("\n[Test] Boundary cache sizes\n");
  
  struct arc_s *cache0 = arc_create(&test_ops, 0);
  TEST_ASSERT(cache0 != NULL, "arc_create with size=0 succeeds");
  
  struct arc_object_s *obj = arc_lookup(cache0, "key1");
  TEST_ASSERT(obj != NULL, "arc_lookup works with size=0 cache");
  
  arc_destroy(cache0);
  
  struct arc_s *cache1 = arc_create(&test_ops, 1);
  TEST_ASSERT(cache1 != NULL, "arc_create with size=1 succeeds");
  
  arc_lookup(cache1, "key1");
  arc_lookup(cache1, "key2");
  arc_lookup(cache1, "key3");
  
  arc_destroy(cache1);
  TEST_ASSERT(true, "arc_destroy completes without crash");
}

static void test_duplicate_key(void) {
  printf("\n[Test] Duplicate key lookup\n");
  destroy_count = 0;
  fetch_count = 0;
  
  struct arc_s *cache = arc_create(&test_ops, 5);
  
  struct arc_object_s *obj1 = arc_lookup(cache, "same_key");
  TEST_ASSERT(obj1 != NULL, "First lookup returns object");
  
  struct arc_object_s *obj2 = arc_lookup(cache, "same_key");
  TEST_ASSERT(obj2 != NULL, "Second lookup returns object");
  TEST_ASSERT(obj1 == obj2, "Same key returns same object (hit)");
  
  TEST_ASSERT(arc_size(cache) == 1, "Cache has only 1 object");
  
  arc_destroy(cache);
  TEST_ASSERT(destroy_count == 1, "Only one object destroyed");
}

static void test_cache_hit_miss(void) {
  printf("\n[Test] Cache hit/miss tracking\n");
  destroy_count = 0;
  evacuate_count = 0;
  fetch_count = 0;
  
  struct arc_s *cache = arc_create(&test_ops, 3);
  
  arc_lookup(cache, "a");
  arc_lookup(cache, "b");
  arc_lookup(cache, "c");
  
  TEST_ASSERT(arc_size(cache) == 3, "3 objects in cache after 3 inserts");
  
  arc_lookup(cache, "a");
  arc_lookup(cache, "b");
  
  TEST_ASSERT(arc_size(cache) == 3, "Cache size unchanged on hits");
  
  arc_destroy(cache);
  TEST_ASSERT(destroy_count == 3, "All 3 objects destroyed");
}

static void test_overflow_eviction(void) {
  printf("\n[Test] Overflow and eviction\n");
  destroy_count = 0;
  evacuate_count = 0;
  
  struct arc_s *cache = arc_create(&test_ops, 3);
  
  arc_lookup(cache, "key1");
  arc_lookup(cache, "key2");
  arc_lookup(cache, "key3");
  
  TEST_ASSERT(arc_size(cache) <= 3, "Cache size respects limit");
  
  arc_lookup(cache, "key4");
  
  TEST_ASSERT(evacuate_count > 0, "Evacuation occurs on overflow");
  
  arc_destroy(cache);
  TEST_ASSERT(destroy_count >= 3, "Destroy cleans up all objects");
}

static void test_ghost_list_behavior(void) {
  printf("\n[Test] Ghost list behavior\n");
  destroy_count = 0;
  evacuate_count = 0;
  
  struct arc_s *cache = arc_create(&test_ops, 3);
  
  arc_lookup(cache, "a");
  arc_lookup(cache, "b");
  arc_lookup(cache, "c");
  arc_lookup(cache, "d");
  
  unsigned int size_before = arc_size(cache);
  TEST_ASSERT(size_before <= 3, "Cache limited after overflow");
  
  arc_lookup(cache, "a");
  arc_lookup(cache, "b");
  
  arc_destroy(cache);
  TEST_ASSERT(destroy_count >= 4, "All objects including ghosts destroyed");
}

static void test_stress(void) {
  printf("\n[Test] Stress test - many keys\n");
  destroy_count = 0;
  
  struct arc_s *cache = arc_create(&test_ops, 10);
  char key[32];
  
  for (int i = 0; i < 100; i++) {
    snprintf(key, sizeof(key), "key_%d", i);
    arc_lookup(cache, key);
  }
  
  TEST_ASSERT(arc_size(cache) <= 10, "Cache size stays within limit");
  
  for (int i = 0; i < 100; i++) {
    snprintf(key, sizeof(key), "key_%d", i);
    struct arc_object_s *obj = arc_lookup(cache, key);
    TEST_ASSERT(obj != NULL, "Previously created keys still retrievable");
  }
  
  arc_destroy(cache);
  TEST_ASSERT(destroy_count > 0, "Objects destroyed on cleanup");
}

static void test_arc_size(void) {
  printf("\n[Test] arc_size function\n");
  
  struct arc_s *cache = arc_create(&test_ops, 5);
  
  TEST_ASSERT(arc_size(cache) == 0, "Initial size is 0");
  
  arc_lookup(cache, "a");
  TEST_ASSERT(arc_size(cache) == 1, "Size 1 after 1 insert");
  
  arc_lookup(cache, "b");
  TEST_ASSERT(arc_size(cache) == 2, "Size 2 after 2 inserts");
  
  arc_destroy(cache);
}

/***** Demo: Basic ARC Behavior *****/

static void demo_basic_behavior(void) {
  printf("\n[Demo] Basic ARC behavior\n");
  
  struct arc_s *cache = arc_create(&test_ops, 10);
  
  printf("\n--- Phase 1: Fill cache ---\n");
  for (int i = 0; i < 5; i++) {
    char key[32];
    snprintf(key, sizeof(key), "key_%d", i);
    arc_lookup(cache, key);
  }
  arc_dump(cache);
  
  printf("\n--- Phase 2: Access frequently ---\n");
  for (int i = 0; i < 3; i++) {
    arc_lookup(cache, "key_0");
    arc_lookup(cache, "key_1");
  }
  arc_dump(cache);
  
  printf("\n--- Phase 3: Overflow ---\n");
  for (int i = 5; i < 15; i++) {
    char key[32];
    snprintf(key, sizeof(key), "key_%d", i);
    arc_lookup(cache, key);
  }
  arc_dump(cache);
  
  arc_destroy(cache);
}

int main(int argc, char *argv[]) {
  printf("=== ARC Functional Tests ===\n");
  
  test_create_destroy();
  test_null_ops();
  test_boundary_size();
  test_duplicate_key();
  test_cache_hit_miss();
  test_overflow_eviction();
  test_ghost_list_behavior();
  test_stress();
  test_arc_size();
  
  printf("\n=== Results: %d passed, %d failed ===\n",
         tests_passed, tests_failed);
  
  if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
    demo_basic_behavior();
  }
  
  return tests_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
