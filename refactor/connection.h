//
// Created by kamenev on 03.12.15.
//

#ifndef POLL_EVENT_CLIENT_H
#define POLL_EVENT_CLIENT_H


#include <stdint.h>
#include <sys/epoll.h>
#include <memory>
#include "io_service.h"
#include "address.h"
#define allFlags (EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLRDHUP|EPOLLHUP)
#define errFlags (EPOLLERR|EPOLLRDHUP|EPOLLHUP)
class connection
{

public:
    typedef std::function<void()> callback;
    void setOn_read(const callback &_on_read)
    {
        on_read = _on_read;
        syncIO();
    }
    void setOn_write(const callback &_on_write)
    {
        on_write = _on_write;
        syncIO();
    }
    int getFd() const
    {
        return fd;
    }

    void setFd(int fd)
    {
        this->fd = fd;
    }
    connection(int, io::io_service &, std::function<void()>);
    ssize_t read_over_connection(void *data, size_t size);
    size_t write_over_connection(void const *data, size_t size);
    void write_all_over_connection(const char *data, size_t size);
    int get_available_bytes();
    static connection connect(io::io_service& ep, ipv4_endpoint const& remote, callback on_disconnect);
    void forceDisconnect();
protected:
    void syncIO();
    callback on_read;
    callback on_write;
    callback on_disconnect;
    std::shared_ptr<io::io_entry> ioEntry;
    int fd;
};
#endif //POLL_EVENT_CLIENT_H
