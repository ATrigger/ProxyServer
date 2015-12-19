#ifndef _POLL_H
#define _POLL_H

#include <unordered_map>
#include <sys/epoll.h>
#include <functional>

#define MAX_EVENTS 100

#define CALLBACK(x) std::function<void (io_element_t *,epoll_event)> x

#define ACCEPT_CB 0x01
#define CONNECT_CB 0x02
typedef struct io_element io_element_t;
typedef struct io_service io_service_t;
typedef std::unordered_map<int, io_element_t *> hashmap;


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

const std::size_t poll_event_element_s = sizeof(io_element_t);

struct io_service {

    hashmap table;

    std::function<int(io_service_t *)> timeout_callback;


    size_t timeout;

    int epoll_fd;

    void *data;

    io_service(int);
};

const std::size_t poll_event_s = sizeof(io_service_t);




io_service_t *poll_event_new(int);

void poll_event_delete(io_service_t *);


int poll_event_add(io_service_t *io,int fd,io_element_t *elem = NULL);


int poll_event_remove(io_service_t *, int);


int poll_event_process(io_service_t *);


void poll_event_loop(io_service_t &);

#define io_start(x) poll_event_loop(x)

#endif
