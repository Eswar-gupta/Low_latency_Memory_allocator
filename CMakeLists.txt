cmake_minimum_required(VERSION 3.16)
project(CustomMemoryAllocator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(allocator_demo src/main.cpp)
target_include_directories(allocator_demo PRIVATE include)

add_executable(allocator_tests tests/allocator_tests.cpp)
target_include_directories(allocator_tests PRIVATE include)

add_executable(benchmark_allocators benchmarks/benchmark_allocators.cpp)
target_include_directories(benchmark_allocators PRIVATE include)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(allocator_demo PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(allocator_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(benchmark_allocators PRIVATE -Wall -Wextra -Wpedantic)
endif()
