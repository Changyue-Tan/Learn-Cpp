cmake_minimum_required(VERSION 3.10)
project(Cpp23_Demos CXX)

set(CMAKE_CXX_STANDARD 23)

# Each .cpp in src/ is a standalone demo with its own main().
file(GLOB CPP23_DEMO_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

foreach(src ${CPP23_DEMO_SOURCES})
    get_filename_component(name "${src}" NAME_WE)
    add_executable("cpp23_${name}" "${src}")
endforeach()

