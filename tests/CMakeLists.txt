include_directories("${PROJECT_SOURCE_DIR}/include/")
link_libraries(ipcxx)

add_executable(test_async_queue test_async_queue.cpp)
add_executable(test_event_loop test_event_loop.cpp)
add_executable(test_thread_pool test_thread_pool.cpp)
