//
// Created by kamenev on 06.12.15.
//

#include <unistd.h>
#include <sys/epoll.h>
#include "events.h"
events::events(io::io_service &service, events::callback _callback)
    : on_ready(std::move(_callback)),
      ioEntry(service, createfd(), EPOLLIN, [this](uint32_t)
      {
          uint64_t res;
          read(fd, &res, sizeof(res));
          this->on_ready(res);
      })
{
    fd=ioEntry.getFd();
}
void events::add(uint64_t i)
{
    write(fd, &i, sizeof(i));
}
void events::setCallback(events::callback callback)
{
    on_ready = std::move(callback);
}
int events::createfd()
{
    int res = eventfd(0, EFD_NONBLOCK);
    if (res == -1) {
        throw_error(errno, "eventfd()");
    }
    return res;
}