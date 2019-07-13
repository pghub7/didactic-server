cmake_minimum_required(VERSION 3.6)   # CMake version check
project(a4_n8c2b_y8y0b)               # Create project "simple_example"
set(CMAKE_CXX_STANDARD 14)            # Enable c++11 standard

set(SOURCE_FILES CSftp.c
        dir.c
        dir.h
        usage.c
        usage.h
        netbuffer.c
        netbuffer.h
        server.c
        server.h
        Makefile)            # Add main.cpp file of project root directory as source file
add_executable(simple_example ${SOURCE_FILES})       # Add executable target with source files listed in SOURCE_FILES variable