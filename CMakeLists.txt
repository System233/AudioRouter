cmake_minimum_required(VERSION "3.12")
project(kcp)
set(CMAKE_CXX_FLAGS "-static")
add_library(kcp SHARED
        src/ikcp.c
        src/ikcp.h
        src/ikcp.hpp
        )
target_link_libraries(kcp wsock32 Ws2_32)


add_executable(kcp_s src/main.cpp)
target_link_libraries(kcp_s kcp winmm ole32)

add_executable(kcp_c src/client.cpp)
target_link_libraries(kcp_c kcp)

