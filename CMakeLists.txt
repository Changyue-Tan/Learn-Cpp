cmake_minimum_required(VERSION 3.10)

# Prefer GCC 15 when available (e.g. C++23 <print> in libstdc++).
if(NOT CMAKE_CXX_COMPILER)
  find_program(LEARN_CPP_GXX15 NAMES g++-15)
  if(LEARN_CPP_GXX15)
    set(CMAKE_CXX_COMPILER "${LEARN_CPP_GXX15}" CACHE FILEPATH "C++ compiler" FORCE)
  endif()
endif()

project(LearnCpp CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Most of this repo is micro-benchmarks. For single-config generators (Ninja/Make),
# default to Release so results aren’t dominated by -O0 overhead.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(_TOP "${CMAKE_CURRENT_SOURCE_DIR}/topics")

# --- Single-file demos by pillar (pthread for threading-heavy samples) ---
function(learn_cpp_add_demos _dir)
  foreach(_demo IN LISTS ARGN)
    add_executable(${_demo} "${_dir}/${_demo}.cpp")
    target_link_libraries(${_demo} PRIVATE pthread)
    if(_demo STREQUAL "hot_loop_optimization")
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|i386")
        target_compile_options(${_demo} PRIVATE -msse2)
      endif()
    endif()
  endforeach()
endfunction()

learn_cpp_add_demos("${_TOP}/concurrency"
  thread_safe_queue
  lockfree_spsc_ring_buffer
  thread_pool
  reader_writer_lock
  lockfree_stack
  bounded_blocking_queue
  waitfree_spsc_queue
  fork_mmap_ipc_sync
  context_switch_threads_vs_processes
  notes_spsc_bump_vs_double_ring
)

learn_cpp_add_demos("${_TOP}/memory"
  fixed_size_memory_pool
  slab_allocator
  lru_cache
)

learn_cpp_add_demos("${_TOP}/microarchitecture"
  cache_friendly_hash_table
  false_sharing
  hot_loop_optimization
  working_set_tlb_huge_pages
)

learn_cpp_add_demos("${_TOP}/io"
  rate_limiter_token_bucket
  timer_wheel
  tcp_echo_server_nonblocking
  event_loop_epoll_kqueue
  async_logger
)

learn_cpp_add_demos("${_TOP}/domain_sketches"
  market_data_message_queue
  order_book_matching_engine
  hft_capstone_pipeline
)

add_executable(aos_vs_soa_particles "${_TOP}/microarchitecture/aos_vs_soa_particles.cpp")

# --- C++20/23 language demos (one executable per *.cpp) ---
set(LANG_DIR "${_TOP}/language")
file(GLOB LANG_DEMO_SOURCES CONFIGURE_DEPENDS "${LANG_DIR}/*.cpp")
list(FILTER LANG_DEMO_SOURCES EXCLUDE REGEX "/mdspan\\.cpp$")
foreach(src IN LISTS LANG_DEMO_SOURCES)
  get_filename_component(name "${src}" NAME_WE)
  add_executable("cpp23_${name}" "${src}")
endforeach()

# --- Appendix: small libraries & linked executables ---
set(ALGO_DIR "${_TOP}/appendix/classic_algorithms")
add_library(classic_algorithms STATIC "${ALGO_DIR}/search.cpp" "${ALGO_DIR}/sort.cpp")
target_include_directories(classic_algorithms PUBLIC "${ALGO_DIR}")

set(PARALLEL_MATH_DIR "${_TOP}/concurrency/parallel_math")
add_library(parallel_math_lib INTERFACE)
target_include_directories(parallel_math_lib INTERFACE "${PARALLEL_MATH_DIR}")
add_executable(parallel_math_async "${PARALLEL_MATH_DIR}/main.cpp")
target_link_libraries(parallel_math_async PRIVATE parallel_math_lib)
add_executable(parallel_math_threads "${PARALLEL_MATH_DIR}/main2.cpp")
target_link_libraries(parallel_math_threads PRIVATE parallel_math_lib)

set(SC_DIR "${_TOP}/memory/smart_container")
add_library(smart_container_lib INTERFACE)
target_include_directories(smart_container_lib INTERFACE "${SC_DIR}")
add_executable(smart_container_demo "${SC_DIR}/demo.cpp")
target_link_libraries(smart_container_demo PRIVATE smart_container_lib)

# Copy into the source tree so clangd / editors pick it up without extra config.
add_custom_target(learn_cpp_sync_compile_commands ALL
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${CMAKE_BINARY_DIR}/compile_commands.json"
          "${CMAKE_SOURCE_DIR}/compile_commands.json"
  COMMENT "Sync compile_commands.json to project root"
  VERBATIM
)
