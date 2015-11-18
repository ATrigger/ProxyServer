cmake_minimum_required(VERSION 3.3)
project(Poll_Event)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    debug.h
    Makefile
        poll.cpp
    poll.h
    test.cpp)

add_executable(Poll_Event ${SOURCE_FILES})