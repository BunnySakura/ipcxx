cmake_minimum_required(VERSION 3.10)
project(ipcxx)

set(CMAKE_CXX_STANDARD 17)

# === 配置选项 ===
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_STATIC_LIBS "Build static libraries" ON)
option(BUILD_TESTS "Build tests" ON)


# === 构建配置 ===
if (BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED)
endif ()

if (BUILD_STATIC_LIBS)
    add_library(${PROJECT_NAME} STATIC)
endif ()

target_include_directories("${PROJECT_NAME}" PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_sources("${PROJECT_NAME}" PRIVATE
        "${PROJECT_SOURCE_DIR}/src/ipcxx.cpp"
        "${PROJECT_SOURCE_DIR}/src/async_queue.cpp"
        "${PROJECT_SOURCE_DIR}/src/thread_pool.cpp"
)


# === 构建测试 ===
if (BUILD_TESTS)
    add_subdirectory(tests)
endif ()


# === 安装 ===
install(TARGETS ${PROJECT_NAME} DESTINATION "lib/")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/ipcxx/" DESTINATION "include/ipcxx")
