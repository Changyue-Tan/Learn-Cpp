cmake_minimum_required(VERSION 3.20)

project(citadel_reconcile_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(reconcile_bench
  main.cpp
)

target_compile_options(reconcile_bench PRIVATE
  -O3
  -DNDEBUG
  -Wall
  -Wextra
  -pedantic
  -march=native
)

# Enable LTO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_error)
if(_ipo_supported)
  set_property(TARGET reconcile_bench PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

