cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# check if ccache is on system
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "ccache found: ${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "/usr/bin/ccache")
else()
  message(STATUS "ccache not found")
endif()

# check if use g++ unless wagie
set(CMAKE_CXX_COMPILER "/usr/bin/g++")

# set(CMAKE_VERBOSE_MAKEFILE ON)

# Export the compilation commands to a file
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(redis)

# Create the build directory
set(BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
if(NOT EXISTS ${BUILD_DIR})
    file(MAKE_DIRECTORY ${BUILD_DIR})
endif()

# Add options to the compilation of source files
add_compile_options(-std=c++20 -Wall -Wextra -Weffc++ -Wpedantic -Wconversion -Wshadow -Wnull-dereference -Wuninitialized -Wuninitialized -Wstrict-aliasing -Wpessimizing-move -Wredundant-move -Wcast-qual -Wduplicated-branches -Wduplicated-cond -Wfloat-equal -Wuninitialized -Winit-self -Wlogical-op -Wpadded -Wswitch-default -Wswitch-enum -Wundef -Wwrite-strings -Wdangling-else -Wunreachable-code)

# build class files
add_library(util STATIC util.cpp)

# Create the executable
add_executable(server server.cpp)
add_executable(client client.cpp)
add_executable(hashmap_test hashmap_test.cpp)

# link to class files
target_link_libraries(server util)
target_link_libraries(client util)
target_link_libraries(hashmap_test util)

# Set the output directory for the executable
set_target_properties(server client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
