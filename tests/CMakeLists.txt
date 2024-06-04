include_directories("${PROJECT_SOURCE_DIR}/include/")
link_libraries(ipcxx)

add_executable(test_async_queue test_async_queue.cpp)
