//
// Created by kamenev on 06.12.15.
//

#include <unistd.h>
#include <sys/epoll.h>
#include "events.h"
#include "posix_sockets.h"
events::events(io::io_service &service, events::callback _callback)
    : on_ready(std::move(_callback)),fd(createfd(true)),
      ioEntry(service, fd, EPOLLIN, [this](uint32_t)
      {
          uint64_t res;
          read_some(fd, &res, sizeof(res));
          this->on_ready(res);
      })
{
}
events::events(io::io_service &service,bool semaphore, events::callback _callback)
    : on_ready(std::move(_callback)),fd(createfd(semaphore)),
      ioEntry(service, fd, EPOLLIN, [this](uint32_t)
      {
          uint64_t res;
          read_some(fd, &res, sizeof(res));
          this->on_ready(res);
      })
{
}
void events::add(uint64_t i)
{
    write_some(fd, &i, sizeof(i));
}
void events::setCallback(events::callback callback)
{
    on_ready = std::move(callback);
}
handle events::createfd(bool semaphore)
{
    int res = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC |(semaphore)?(EFD_SEMAPHORE):(0));
    if (res == -1) {
        throw_error(errno, "eventfd()");
    }
    return res;
}