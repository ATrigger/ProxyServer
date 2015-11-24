#ifndef _POLL_H
#define _POLL_H

#include <unordered_map>
#include <sys/epoll.h>
#include <functional>

#define MAX_EVENTS 100

#define CALLBACK(x) std::function<void (io_element *,epoll_event)> x

#define ACCEPT_CB 0x01
#define CONNECT_CB 0x02
typedef struct io_service io_service;
typedef struct io_element io_element;
typedef std::unordered_map<int, io_element *> hashmap;


struct io_element {

    int fd;

    CALLBACK(write_callback);

    CALLBACK(read_callback);

    CALLBACK(close_callback);

    CALLBACK(accept_callback);

    CALLBACK(connect_callback);

    void *data;

    uint32_t events;

    uint32_t cur_event;

    uint8_t cb_flags;
    io_service *io;
    void sc_flag(int flag);
    void add_flag(int flag);

    void remove_flag(int flag);

    void io_element_delete();


    io_element(int, uint32_t, io_service *,std::unordered_map <std::string,CALLBACK()>);


};

const std::size_t poll_event_element_s = sizeof(io_element);

struct io_service {

    hashmap table;

    std::function<int(io_service *)> timeout_callback;


    size_t timeout;

    int epoll_fd;

    void *data;
    void setTimeoutCallback(std::function<int(io_service *)>);
    io_service(int);
    int add_event(int,io_element* =NULL);
    int remove_event(int);
    int process();
    void loop();
};
const std::size_t poll_event_s = sizeof(io_service);
void poll_event_delete(io_service *);
#define io_start(x) x.loop()

#endif
