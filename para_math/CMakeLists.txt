cmake_minimum_required(VERSION 3.10)
project(ParaMath CXX)

set(CMAKE_CXX_STANDARD 23)

add_library(para_math_lib INTERFACE)
target_include_directories(para_math_lib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(para_math_async main.cpp)
target_link_libraries(para_math_async PRIVATE para_math_lib)

add_executable(para_math_threads main2.cpp)
target_link_libraries(para_math_threads PRIVATE para_math_lib)

