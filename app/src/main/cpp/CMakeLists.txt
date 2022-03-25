cmake_minimum_required(VERSION 3.18)
project(kcp)
set(CMAKE_BUILD_TYPE Debug)
include_directories(include)
add_library(kcp SHARED
        src/main.cpp
        src/ikcp.c
        src/ikcp.h
        src/ikcp.hpp)
target_link_libraries(kcp log)
