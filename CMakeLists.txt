cmake_minimum_required(VERSION 3.20.0)

if(WIN32)
    message(FATAL_ERROR "WINDOWS NOT SUPPORTED")
endif()

project(stockex VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
string(APPEND CMAKE_CXX_FLAGS "-Wall -Wextra -Werror -Wpedantic -march=haswell")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(NoInSourceBuilds)
include(Libuv)

include_directories(${PROJECT_SOURCE_DIR}/src)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(bench)

add_executable(stockex src/main.cpp)
