#ifndef _IOSERVICE_H
#define _IOSERVICE_H

#include <unordered_map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mutex>
#include <functional>
#include "debug.h"
#define MAX_EVENTS 100
struct io_element;
struct acceptor;
typedef std::unordered_map<int, io_element *> hashmap;
struct io_service {

    hashmap table;
    std::mutex tM;
    std::function<int(io_service *)> timeout_callback;
    std::unordered_map<std::string,std::string> dns;

    size_t timeout;

    int epoll_fd;

    void *data;

    void setTimeoutCallback(std::function<int(io_service *)>);

    io_service(size_t, std::function<int(io_service *)> func = NULL);

    int addOrUpdate_event(int, io_element * = NULL);

    template <typename T>
    int add_event(T&p);

    int remove_event(int);

    int process();

    void loop();
};

const std::size_t poll_event_s = sizeof(io_service);

void poll_event_delete(io_service *);

template <typename T>
int io_service::add_event(T &p) {
    auto element = p.getpoll().get();
    std::lock_guard<std::mutex> lcg(tM);
    table[element->fd] = element;
    lcg.~lock_guard();
    LOG("Added fd(%d)", element->fd);
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.data.fd = element->fd;
    ev.events = element->events;
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, element->fd, &ev);
}

#define io_start(x) x.loop()
#endif