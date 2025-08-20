cmake_minimum_required(VERSION 4.1.0)

project(C_redis_server)

set(CMAKE_BUILD_TYPE Debug)

file(GLOB SRC_FILS
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/src/*.h
)

add_executable(${PROJECT_NAME} ${SRC_FILS})

# 链接 Windows 套接字库
target_link_libraries(${PROJECT_NAME} PRIVATE lws2_32)