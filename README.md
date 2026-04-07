# Adaptive Replacement Cache (ARC)

## Overview

Adaptive Replacement Cache (ARC) is an efficient caching replacement algorithm designed to improve cache hit rates. ARC combines the strengths of LRU (Least Recently Used) and LFU (Least Frequently Used) to optimize data that is frequently and recently accessed.

## Features

- Utilizes an adaptive replacement strategy that dynamically adjusts to access patterns.
- Supports various operations: creation, lookup, updating, and destruction of cache objects.
- Provides an interface for custom object comparison, fetching, creation, and destruction operations.
- O(1) hash table lookup for fast cache access.
- Ghost lists track recently evicted items for faster workload adaptation.

## Quick Start

### Build and Run Demo

```bash
# Create build directory and configure
mkdir build && cd build

# Build the project
cmake ..
make

# Run the demo
./demo

# Run the benchmark
./benchmark
```

### Basic Usage

```c
#include "arc.h"

typedef struct {
  struct arc_object_s arc_obj;
  char key[32];
  int value;
} my_object_t;

static int my_cmp(const void *key, const void *other_key) {
  return strcmp((const char *)key, (const char *)other_key);
}

static unsigned int my_hash(const void *key, unsigned int hash_size) {
  unsigned int h = 0;
  const unsigned char *s = (const unsigned char *)key;
  while (*s) { h = h * 31 + *s++; }
  return h % hash_size;
}

static void *my_fetch(const void *key, struct arc_object_s *obj) {
  my_object_t *my_obj = (my_object_t *)((char *)obj - offsetof(my_object_t, arc_obj));
  my_obj->value = rand();
  return &my_obj->value;
}

static struct arc_object_s *my_create(const void *key, void *value) {
  my_object_t *obj = malloc(sizeof(my_object_t));
  if (!obj) return NULL;
  memset(obj, 0, sizeof(my_object_t));
  strncpy(obj->key, (const char *)key, sizeof(obj->key) - 1);
  obj->arc_obj.key = obj->key;
  return &obj->arc_obj;
}

static void my_destroy(struct arc_object_s *obj) {
  my_object_t *my_obj = (my_object_t *)((char *)obj - offsetof(my_object_t, arc_obj));
  free(my_obj);
}

static struct arc_ops_s ops = {
  .cmp = my_cmp,
  .hash = my_hash,
  .fetch = my_fetch,
  .create = my_create,
  .destroy = my_destroy,
};

int main(void) {
  struct arc_s *cache = arc_create(&ops, 1000);  // Cache size: 1000

  // Lookup or create
  struct arc_object_s *obj = arc_lookup(cache, "my_key");
  if (obj) {
    printf("Value: %d\n", *(int *)obj->value);
  }

  arc_destroy(cache);
  return 0;
}
```

## Architecture

The ARC algorithm manages five lists:
- **T1 (MRU)**: Most Recently Used - new cache entries
- **T2 (MFU)**: Most Frequently Used - frequently accessed entries
- **B1 (Ghost MRU)**: Ghost list for T1 - tracks evicted MRU items
- **B2 (Ghost MFU)**: Ghost list for T2 - tracks evicted MFU items

The `p` parameter dynamically balances between recency (T1) and frequency (T2).

## Benchmark Results

The benchmark tests compare ARC with LRU and LFU across various access patterns.

### Test Configuration
- Keys: 100,000
- Cache Size: 1,000
- Iterations: 100,000

### Hit Rate Comparison

| Pattern | Description | ARC | LRU | LFU |
|---------|-------------|-----|-----|-----|
| 80/20 | 80% access to 20% hot keys | **7.0%** | 2.7% | 3.2% |
| Loop | Sequential key repetition | 99.0% | 99.0% | 99.0% |
| Mixed | Random mixed access | **31.0%** | 10.1% | 13.3% |
| Scan | Scan resistance test | 97.3% | **100.0%** | **100.0%** |
| Adaptive | Workload change | **100.0%** | **100.0%** | 98.0% |
| Temporal | Temporal locality shift | **48.0%** | 24.0% | 24.0% |

### Scalability (Hit Rate vs Dataset Size)

| Data Size | ARC | LRU | LFU |
|-----------|-----|-----|-----|
| 1,000 | 81.3% | 81.3% | 81.4% |
| 2,000 | **86.1%** | 68.3% | 76.5% |
| 5,000 | **57.4%** | 29.5% | 32.7% |
| 10,000 | **30.7%** | 14.3% | 15.9% |

### Performance (Execution Time)

| Pattern | ARC | LRU | LFU | Speedup |
|---------|-----|-----|-----|---------|
| 80/20 | 18ms | 664ms | 473ms | 37x |
| Loop | 6ms | 411ms | 101ms | 64x |
| Mixed | 19ms | 579ms | 443ms | 30x |
| Scan | 3ms | 93ms | 56ms | 28x |
| Adaptive | 3ms | 109ms | 62ms | 40x |
| Temporal | 16ms | 607ms | 441ms | 38x |

## Key Observations

1. **ARC consistently outperforms LRU/LFU** on mixed and temporal shift patterns
2. **Ghost lists** enable fast recovery after workload changes
3. **Dynamic 'p' parameter** balances recency vs frequency adaptively
4. **Pattern 3** shows ARC's advantage with random mixed access (2-3x LRU)
5. **Scalability**: ARC maintains higher hit rates as dataset grows
6. **O(1) hash lookups** make ARC significantly faster than naive LRU/LFU implementations

## License

MIT License
