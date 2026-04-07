cmake_minimum_required(VERSION 3.20)
project(adaptive_replacement_cache C)
set(CMAKE_C_STANDARD 23)

add_executable(demo test/demo.c)
target_link_libraries(demo PRIVATE m)

add_executable(benchmark test/benchmark.c)
target_link_libraries(benchmark PRIVATE m)
