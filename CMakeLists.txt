cmake_minimum_required(VERSION 3.18)
project(http_server)
include_directories(${CMAKE_SOURCE_DIR}/src/include)
set(SOURCE_FILES
        src/http_server.cpp
        src/include/http_server.h
        src/sockets.cpp
        src/include/sockets.h
        src/main.cpp
        src/http_server.cpp
)
add_executable(http_server ${SOURCE_FILES})
find_package(Threads REQUIRED)
if(WIN32)
    target_link_libraries(http_server Threads::Threads ws2_32 wsock32)
endif()