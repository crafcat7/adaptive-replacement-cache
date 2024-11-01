cmake_minimum_required(VERSION 3.30)
project(adaptive_replacement_cache C)
set(CMAKE_C_STANDARD 23)

add_library(adaptive_replacement_cache STATIC arc.c)

target_include_directories(adaptive_replacement_cache PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_executable(demo test/demo.c)
target_link_libraries(demo PRIVATE adaptive_replacement_cache)
