cmake_minimum_required(VERSION 3.14)
project(sensor_fusion)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# optimizations on, debug symbols for profiling
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g")

add_executable(benchmark src/benchmark.cpp)

target_include_directories(benchmark PRIVATE src)

# link pthreads — required for std::thread on Linux
find_package(Threads REQUIRED)
target_link_libraries(benchmark PRIVATE Threads::Threads)