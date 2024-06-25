cmake_minimum_required(VERSION 3.28)
project(udp_jwatrio_server C)

set(CMAKE_C_STANDARD 11)

add_executable(udp_jwatrio_server
        server.c
)

target_link_libraries(udp_jwatrio_server ws2_32)