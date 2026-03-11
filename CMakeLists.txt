cmake_minimum_required(VERSION 3.10)
project(LearnCpp CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(hft_demos)
add_subdirectory(cpp23_demos)
add_subdirectory(hello_algo)
add_subdirectory(para_math)
add_subdirectory(smart_container)
add_subdirectory(dop_vs_oop)

