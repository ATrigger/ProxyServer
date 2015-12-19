#ifndef _POLL_H
#define _POLL_H

#include <unordered_map>
#include <sys/epoll.h>
#include <functional>
#include <memory>
#include <vector>
#include "io_service.h"

#define CALLBACK(x) std::function<void (io_element *,epoll_event)> x

#define ACCEPT_CB 0x01
#define CONNECT_CB 0x02

typedef struct io_element io_element;
typedef std::unordered_map<int, io_element *> hashmap;
struct io_data{
    std::shared_ptr<io_element> relative;
    std::string rqOrs;
    std::unordered_map<std::string,std::string> parsed;
    void *anydata;

};
struct io_element {

    int fd;

    CALLBACK(write_callback);

    CALLBACK(read_callback);

    CALLBACK(close_callback);

    CALLBACK(accept_callback);

    CALLBACK(connect_callback);

    io_data data;

    uint32_t events;

    uint32_t cur_event;

    uint8_t cb_flags;
    io_service *io;

    void add_flag(int flag);

    void remove_flag(int flag);

    void io_element_delete();


    io_element(int, int, uint32_t, io_service *, std::unordered_map<std::string, CALLBACK() >);


};

const std::size_t poll_event_element_s = sizeof(io_element);


#endif
