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