cmake_minimum_required(VERSION 3.29)
project(teso_vs_procexp)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-DUNICODE -D_UNICODE)

add_subdirectory(common)
add_subdirectory(service)
add_subdirectory(client)
