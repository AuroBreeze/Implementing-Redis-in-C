cmake_minimum_required(VERSION 4.1.0)

project(C_redis_server)

set(CMAKE_BUILD_TYPE Debug)

# 设置所有可执行文件输出到 bin 目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

file(GLOB SRC_FILS
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/src/*.h
)

add_executable(server ${SRC_FILS})

# 链接 Windows 套接字库
target_link_libraries(server PRIVATE ws2_32)

add_executable(client ${PROJECT_SOURCE_DIR}/test/client.cpp)
target_link_libraries(client PRIVATE ws2_32)

add_executable(server_test ${PROJECT_SOURCE_DIR}/test/server_test.cpp)
target_link_libraries(server_test PRIVATE ws2_32)


