cmake_minimum_required(VERSION 3.10)
project(SmartContainerDemo CXX)

set(CMAKE_CXX_STANDARD 23)

add_library(smart_container_lib INTERFACE)
target_include_directories(smart_container_lib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(smart_container_demo demo.cpp)
target_link_libraries(smart_container_demo PRIVATE smart_container_lib)

