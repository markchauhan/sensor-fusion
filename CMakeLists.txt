cmake_minimum_required(VERSION 3.16)
project(sensor_fusion LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Threads REQUIRED)

add_library(sf INTERFACE)
target_include_directories(sf INTERFACE include)
target_compile_options(sf INTERFACE -Wall -Wextra -Wpedantic)
target_link_libraries(sf INTERFACE Threads::Threads)

enable_testing() 
add_executable(test_imu_sample tests/test_imu_sample.cpp)
target_link_libraries(test_imu_sample PRIVATE sf)
add_test(NAME test_imu_sample COMMAND test_imu_sample)

add_executable(test_spsc_ring tests/test_spsc_ring.cpp)
target_link_libraries(test_spsc_ring PRIVATE sf)
add_test(NAME spsc_ring COMMAND test_spsc_ring)

add_executable(test_latency_histogram tests/test_latency_histogram.cpp)
target_link_libraries(test_latency_histogram PRIVATE sf)
add_test(NAME latency_histogram COMMAND test_latency_histogram)