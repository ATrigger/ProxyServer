//
// Created by kamenev on 05.12.15.
//

#include <sys/epoll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "io_service.h"
#include "connection.h"
#include "posix_sockets.h"
#include "debug.h"
connection::connection(int _fd, io::io_service &ep, std::function<void()> end)
    : fd(_fd), on_disconnect(std::move(end)), destroyed(nullptr),
        // TODO: fd can leak if make_shared fails
      ioEntry(std::make_shared<io::io_entry>(ep, _fd, errFlags, [this](uint32_t events)
      {
          bool is_destroyed = false;
          destroyed = &is_destroyed;
          try {
              if (events & EPOLLIN) {
                  on_read();
                  if (is_destroyed) return;
              }
              if (events & errFlags) {
                  on_disconnect();
                  if (is_destroyed) return;
              }
              if (events & EPOLLOUT) {
                  on_write();
                  if (is_destroyed) return;
              }
          }
          catch (...) {
              destroyed = nullptr;
              INFO("EPOLL execution failed");
              __throw_exception_again;
          }
          destroyed = nullptr;
      }))
{

}
void connection::syncIO()
{
    uint32_t flags = EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    if (on_read) flags |= EPOLLIN;
    if (on_write) flags |= EPOLLOUT;

    this->ioEntry->modify(flags);
}
ssize_t connection::read_over_connection(void *data, size_t size)
{
    return read_some(fd, data, size);
}
size_t connection::write_over_connection(void const *data, size_t size)
{
    return write_some(fd, data, size);
}
connection connection::connect(io::io_service &ep, ipv4_endpoint const &remote, connection::callback on_disconnect)
{
    int fd = make_socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK);
    connect_socket(fd, remote.port_net, remote.addr_net);
    connection res{fd, ep, std::move(on_disconnect)};
    return res;

}
void connection::forceDisconnect()
{
    LOG("Forced disconnect on %d fd", getFd());
    on_disconnect();
}
int connection::get_available_bytes() const
{
    int n = -1;
    if (ioctl(fd, FIONREAD, &n) < 0) {
        LOG("IOCTL failed: %d. No bytes available. Returning 0", errno);
        return 0;
    }
    return n;
}
int connection::getFd() const
{
    return fd;
}
void connection::setOn_write(const callback &_on_write)
{
    on_write = _on_write;
    syncIO();
}
void connection::setOn_read(const callback &_on_read)
{
    on_read = _on_read;
    syncIO();
}
connection::~connection()
{
    if (destroyed) *destroyed = true;
}
void connection::sleep()
{
    on_write = callback();
    on_read = callback();
    syncIO();
}
void connection::setOn_rw(const connection::callback &on_read, const connection::callback &on_write)
{
    this->on_read = on_read;
    this->on_write = on_write;
    syncIO();
}
