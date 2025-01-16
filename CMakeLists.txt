cmake_minimum_required(VERSION 3.10)
project(ipcxx
        VERSION 1.0.2
        DESCRIPTION "仅头文件的轻量级C++中间件开发框架。"
        HOMEPAGE_URL "https://github.com/BunnySakura/ipcxx"
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(LIBRARY_NAME "ipcxx")


# === 配置选项 ===
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build tests" ON)


# === 构建配置 ===
if (BUILD_SHARED_LIBS)
    add_library(${LIBRARY_NAME} SHARED)
    add_library(${LIBRARY_NAME}::${LIBRARY_NAME} ALIAS ${LIBRARY_NAME})
    # 设置库的版本号和SOVERSION
    set_target_properties(${LIBRARY_NAME} PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    # 为Debug版本的库添加d后缀
    set_target_properties(${LIBRARY_NAME} PROPERTIES DEBUG_POSTFIX "d")

    target_include_directories("${LIBRARY_NAME}" PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_sources("${LIBRARY_NAME}" PRIVATE
            "${PROJECT_SOURCE_DIR}/src/ipcxx.cpp"
            "${PROJECT_SOURCE_DIR}/src/async_queue.cpp"
            "${PROJECT_SOURCE_DIR}/src/event_loop.cpp"
            "${PROJECT_SOURCE_DIR}/src/thread_pool.cpp"
            "${PROJECT_SOURCE_DIR}/src/async_socket.cpp"
    )
endif ()


# === 构建测试 ===
if (BUILD_TESTS)
    add_subdirectory(tests)
endif ()


# === 安装 ===
if (BUILD_SHARED_LIBS)
    install(TARGETS ${LIBRARY_NAME} DESTINATION "lib/")
endif ()
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/ipcxx/" DESTINATION "include/ipcxx")
