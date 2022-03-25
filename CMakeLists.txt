cmake_minimum_required(VERSION "3.12")
project(kcp)
set(CMAKE_CXX_FLAGS "-static")
add_subdirectory(libs/asio-kcp)
add_executable(kcp_s src/main.cpp)
target_link_libraries(kcp_s asio-kcp winmm ole32 wsock32 Ws2_32)

add_executable(kcp_c src/client.cpp)
target_link_libraries(kcp_c asio-kcp wsock32 Ws2_32)

