add_subdirectory(test)
add_library(thread-pool
  thread-pool.c)
target_link_libraries(thread-pool PRIVATE system)
target_include_directories(thread-pool PUBLIC .)