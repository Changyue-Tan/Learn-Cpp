cmake_minimum_required(VERSION 3.10)
project(HFT_Demos CXX)

set(CMAKE_CXX_STANDARD 23)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# All single-file demos
set(DEMOS
  01_thread_safe_queue
  02_lockfree_spsc_ring_buffer
  03_thread_pool
  04_reader_writer_lock
  05_lockfree_stack
  06_fixed_size_memory_pool
  07_slab_allocator
  08_lru_cache
  09_rate_limiter_token_bucket
  10_timer_wheel
  11_bounded_blocking_queue
  12_waitfree_spsc_queue
  13_cache_friendly_hash_table
  14_tcp_echo_server_nonblocking
  15_event_loop_epoll_kqueue
  16_async_logger
  17_false_sharing
  18_hot_loop_optimization
  19_market_data_message_queue
  20_order_book_matching_engine
  21_fork_mmap_ipc_sync
  22_context_switch_threads_vs_processes
  23_working_set_tlb_huge_pages
)

foreach(demo ${DEMOS})
  add_executable(${demo} ${demo}.cpp)
  target_link_libraries(${demo} PRIVATE pthread)
  if(APPLE AND demo STREQUAL "14_tcp_echo_server_nonblocking")
    # no extra libs
  endif()
  if(demo STREQUAL "18_hot_loop_optimization")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|i386")
      target_compile_options(${demo} PRIVATE -msse2)
    endif()
  endif()
endforeach()
