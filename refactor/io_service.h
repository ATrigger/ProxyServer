//
// Created by kamenev on 03.12.15.
//

#ifndef POLL_EVENT_IO_SERVICE_H
#define POLL_EVENT_IO_SERVICE_H
#include <unordered_map>
#include <functional>
#include <memory>
#include "timer.h"
#include "handle.h"

class connection;
class acceptor;
#define MAX_EVENTS 1
namespace io
{
class io_entry;
class io_service
{
    friend class io_entry;
    void default_timeout();
    std::function<int()> timeout;
    size_t timeoutMS = 1000;
    int epoll;
    void *holder;
    timer::timer_service clock;
    int calculate_timeout();
    int loop();
public:
    io_service();
    io_service(size_t, std::function<int()> func = NULL);
    void setCallback(std::function<int()>);
    void control(handle&, int, uint32_t, io_entry *);
    void removefd(handle&);
    int run();
    void setHolder(void *holder);
    void *getHolder() const;
    timer::timer_service& getClock();
};
class io_entry
{
    friend class io_service;
public:
    io_entry(io_service &, handle&, uint32_t, std::function<void(uint32_t)>);
    void modify(uint32_t);
    io_service &getparent();
    ~io_entry();
private:
    void sync();
public:
    handle& getFd() const
    {
        return fd;
    }
private:
    handle& fd;
    io_service *parent;
    uint32_t events;
    std::function<void(uint32_t)> callback;
};
}
#endif //POLL_EVENT_IO_SERVICE_H
