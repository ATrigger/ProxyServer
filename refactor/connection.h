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
// TODO: make this define const
#define errFlags (EPOLLERR|EPOLLRDHUP|EPOLLHUP)
class connection
{

public:
    connection(int, io::io_service &, std::function<void()>);
    typedef std::function<void()> callback;
    void setOn_read(const callback &_on_read);
    void setOn_write(const callback &_on_write);
    void setOn_rw(const callback &_on_read, const callback &on_write);
    void sleep();
    int getFd() const;
    ~connection();
    ssize_t read_over_connection(void *data, size_t size);
    size_t write_over_connection(void const *data, size_t size);
    int get_available_bytes() const;
    static connection connect(io::io_service& ep, ipv4_endpoint const& remote, callback on_disconnect);
    void forceDisconnect();
protected:
    bool *destroyed;
    void syncIO();
    callback on_read;
    callback on_write;
    callback on_disconnect;
    // TODO: check if shared_ptrs are neccessary
    std::shared_ptr<io::io_entry> ioEntry;
    int fd;
};
#endif //POLL_EVENT_CLIENT_H
