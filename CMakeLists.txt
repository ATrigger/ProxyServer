cmake_minimum_required(VERSION 3.3)
project(Poll_Event)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS system thread)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG -g -O0 -std=c++11")

set(SOURCE_FILES
        debug.h

        io_element.cpp
        io_service.cpp
        io_service.h
        io_element.h
        test.cpp server.cpp server.h client.cpp client.h )
set(NEW_SOURCE
        refactor/address.cpp
        refactor/address.h
        refactor/debug.h
        refactor/HTTP.h
        refactor/io_service.h
        refactor/io_service.cpp
        refactor/acceptor.cpp
        refactor/acceptor.h
        refactor/epoll_error.cpp
        refactor/main_proxy.cpp
        refactor/outstring.cpp
        refactor/outstring.h
        refactor/epoll_error.h
        refactor/posix_sockets.cpp refactor/posix_sockets.h refactor/connection.h refactor/connection.cpp refactor/proxy_server.cpp refactor/proxy_server.h refactor/main_proxy.cpp refactor/events.cpp refactor/events.h refactor/HTTP.cpp)
add_executable(OLD ${SOURCE_FILES})
add_executable(NEW ${NEW_SOURCE})
target_link_libraries(NEW ${Boost_LIBRARIES})