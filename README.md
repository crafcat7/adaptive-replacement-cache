# Adaptive Replacement Cache (ARC)

## Overview

Adaptive Replacement Cache (ARC) is an efficient caching replacement algorithm designed to improve cache hit rates. ARC combines the strengths of LRU (Least Recently Used) and LFU (Least Frequently Used) to optimize data that is frequently and recently accessed.

## Features

- Utilizes an adaptive replacement strategy that dynamically adjusts to access patterns.
- Supports various operations: creation, lookup, updating, and destruction of cache objects.
- Provides an interface for custom object comparison, fetching, creation, and destruction operations.

## Run Demo

1. Clone the repository:
```
git clone https://github.com/crafcat7/adaptive-replacement-cache.git
cd adaptive-replacement-cache
```

2. Create a build directory and navigate to it:
```
mkdir build
cd build
```
3. Build and run the project:
```
cmake ..
make
./demo
```

4. Expectation result
   In the demo, we will loop through 20 entries with different keys (our cache capacity is 10 entries), and will frequently access key_00 and key_01, so it will eventually appear in the MFU List.
```
=======================================
create demo_obj:0x600002a78570 create, key:example_key_20, data:create
MRU List size[9]:
arc_object: 0x600002a78570
arc_object: 0x600002a78540
arc_object: 0x600002a78510
arc_object: 0x600002a784e0
arc_object: 0x600002a784b0
arc_object: 0x600002a78480
arc_object: 0x600002a78450
arc_object: 0x600002a78420
arc_object: 0x600002a783f0
MFU List size[2]:
arc_object: 0x600002a7c030
arc_object: 0x600002a7c000
GMFU List size[0]:
List is empty.
GMRU List:[10]:
arc_object: 0x600002a783c0
arc_object: 0x600002a78390
arc_object: 0x600002a78360
arc_object: 0x600002a78330
arc_object: 0x600002a78300
arc_object: 0x600002a782d0
arc_object: 0x600002a782a0
arc_object: 0x600002a78270
arc_object: 0x600002a7c090
arc_object: 0x600002a7c060
=======================================
```