//
// Created by kamenev on 03.12.15.
//

#include <sys/epoll.h>
#include <unistd.h>
#include "io_service.h"
#include "debug.h"
#include "epoll_error.h"

io::io_service::io_service(size_t timeoutMS, std::function<int()> func)
    : timeoutMS(timeoutMS), timeout(func)
{
    epoll = epoll_create(MAX_EVENTS);
    INFO("IO_SERVICE created");
}
io::io_service::io_service()
    : io_service(1000, nullptr)
{

}

void io::io_service::default_timeout()
{
    static int i = 0;
    i++;
    if(i%10==0)LOG("Timeouted: %d times.",i);
}

void io::io_service::control(handle& fd, int operation, uint32_t flags, io_entry *to)
{
    epoll_event event;
    event.data.ptr = to;
    event.events = flags;
    int res = epoll_ctl(epoll, operation, fd.get_raw(), &event);
    if (res < 0)
        throw_error(errno, "EPOLL_CTL");
}

int io::io_service::run()
{
    while (loop()==0);
    return 0;
}

int io::io_service::loop()
{
    epoll_event events[MAX_EVENTS];
    int count;
    int nearest_timer = calculate_timeout();
    timeoutMS = (nearest_timer<0)?(1000):(nearest_timer);
    do {
        count = epoll_wait(epoll, events, MAX_EVENTS, timeoutMS);
    }
    while (count < 0 && errno == EINTR);
    if(count < 0){
        throw_error(errno,"epoll_wait()");
    }
    if (count == 0) {
        if (timeout) {
            if (timeout() !=0)
                return 1;
        }
        else {
            default_timeout();
            return 0;
        }
    }
    for(int i=0;i<count;++i){
        auto &ee = events[i];
        try {
            static_cast<io_entry *>(ee.data.ptr)->callback(ee.events);
        }
        catch(std::exception &e){
            LOG("%s happened on EPOLL execution",e.what());
        }
        catch(...){
            INFO("Something happened on EPOLL execution");
        }
    }
    return 0;
}

void io::io_service::removefd(handle& i)
{
    int res = epoll_ctl(epoll, EPOLL_CTL_DEL, i.get_raw(), nullptr);
    if (res < 0)throw_error(errno, "EPOLL_DEL");
}

void *io::io_service::getHolder() const
{
    return holder;
}

void io::io_service::setHolder(void *holder)
{
    io_service::holder = holder;
}

io::io_entry::io_entry(io::io_service &service, handle& fd, uint32_t flags, std::function<void(uint32_t)> function)
    : fd(fd), events(flags), parent(&service), callback(function)
{
    service.control(this->fd, EPOLL_CTL_ADD, flags, this);
}
void io::io_entry::modify(uint32_t flags)
{
    if (flags == events) return;
    events = flags;
    sync();
}
io::io_service &io::io_entry::getparent()
{
    return *parent;
}
void io::io_entry::sync()
{
    if (parent) {
        parent->control(fd, EPOLL_CTL_MOD, events, this);
    }
}
io::io_entry::~io_entry()
{
    if(parent) parent->removefd(this->fd);
}
void io::io_service::setCallback(std::function<int()> function)
{
    timeout = std::move(function);
}
int io::io_service::calculate_timeout()
{
    if(clock.empty()) return -1;

    timer::timer_service::clock_t::time_point now = timer::timer_service::clock_t::now();
    clock.process(now);
    if(clock.empty()) return -1;

    return std::chrono::duration_cast<std::chrono::milliseconds>(clock.top()-now).count();
}
io::timer::timer_service &io::io_service::getClock()
{
    return clock;
}
