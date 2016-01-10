//
// Created by kamenev on 06.12.15.
//

#ifndef POLL_EVENT_EVENTS_H
#define POLL_EVENT_EVENTS_H
#include <functional>
#include <sys/eventfd.h>
#include "io_service.h"
#include "epoll_error.h"

class events
{
public:
    typedef std::function<void (uint64_t)> callback;
    events(io::io_service &service, bool semaphore, callback _callback);
    events(io::io_service&, callback);
    void add(uint64_t i=1);
    void setCallback(callback);
    int createfd(bool);
private:
    io::io_entry ioEntry;
    callback on_ready;
private:
    int fd;
};


#endif //POLL_EVENT_EVENTS_H
