cmake_minimum_required(VERSION 3.18)
project(WebServer)
set(BUILD_EXAMPLES ON)
set(CMAKE_CXX_STANDARD 11)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/include)
set(SOURCE_FILES
        src/http_server.cpp
        src/include/http_server.h
        src/sockets.cpp
        src/include/sockets.h
        src/http_server.cpp
)
add_library(WebServer ${SOURCE_FILES})
find_package(Threads REQUIRED)
if (WIN32)
    target_link_libraries(WebServer Threads::Threads ws2_32 wsock32)
endif ()
#if (BUILD_EXAMPLES)
#    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples/example_static)
#    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples/example_dynamic)
#    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples/example_parameters)
#    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples/example_ajax)
#    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples/example_forms)
#    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples/example_sse)
#endif ()