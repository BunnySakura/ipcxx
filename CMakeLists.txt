cmake_minimum_required(VERSION 3.20)
project(ipcxx)

set(CMAKE_CXX_STANDARD 17)

add_library(${PROJECT_NAME} SHARED)

target_include_directories("${PROJECT_NAME}" PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_sources("${PROJECT_NAME}" PRIVATE
        "${PROJECT_SOURCE_DIR}/src/ipcxx.cpp"
        "${PROJECT_SOURCE_DIR}/src/async_queue.cpp"
        "${PROJECT_SOURCE_DIR}/src/thread_pool.cpp"
)


install(TARGETS ${PROJECT_NAME} DESTINATION "lib/")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/ipcxx/" DESTINATION "include/ipcxx")
