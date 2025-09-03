cmake_minimum_required(VERSION 4.1.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(C_redis_server)

# 设置所有可执行文件输出到 bin 目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

file(GLOB SRC_FILS
    ${PROJECT_SOURCE_DIR}/src/server.cpp
    ${PROJECT_SOURCE_DIR}/src/hashtable.cpp
    ${PROJECT_SOURCE_DIR}/src/zset.cpp
    ${PROJECT_SOURCE_DIR}/src/avl.cpp
)

set(CMAKE_BUILD_TYPE Debug)

add_executable(server ${SRC_FILS})

# 链接 Windows 套接字库
target_link_libraries(server PRIVATE ws2_32)

add_executable(client ${PROJECT_SOURCE_DIR}/test/client.cpp)
target_link_libraries(client PRIVATE ws2_32)

add_executable(server_test ${PROJECT_SOURCE_DIR}/test/server_test.cpp)
target_link_libraries(server_test PRIVATE ws2_32)

add_executable(test_avl ${PROJECT_SOURCE_DIR}/src/test_avl.cpp ${PROJECT_SOURCE_DIR}/src/avl.cpp)

add_executable(test_offset ${PROJECT_SOURCE_DIR}/src/test_offset.cpp ${PROJECT_SOURCE_DIR}/src/avl.cpp)


