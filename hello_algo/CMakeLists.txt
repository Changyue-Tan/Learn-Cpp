cmake_minimum_required(VERSION 3.10)
project(HelloAlgo CXX)

set(CMAKE_CXX_STANDARD 23)

add_library(hello_algo STATIC
    search.cpp
    sort.cpp
)

target_include_directories(hello_algo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

