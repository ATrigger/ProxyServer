//
// Created by kamenev on 03.12.15.
//

#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "acceptor.h"
#include "epoll_error.h"
#include "posix_sockets.h"
#include "debug.h"

acceptor::acceptor(io::io_service &ep, const ipv4_endpoint &endpoint, std::function<void()> on_accept)
    : fd(make_socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK)),
        accept_connection(on_accept), ioEntry(ep, fd, EPOLLIN,
                                      [this](uint32_t event)
                                      {
                                          if (event == EPOLLIN) this->accept_connection();
                                      })
{
    bind_socket(fd, endpoint.port_net, endpoint.addr_net);
    start_listen(fd);

}

ipv4_endpoint acceptor::local_endpoint() const
{
    sockaddr_in saddr{};
    socklen_t saddr_len = sizeof saddr;
    int res = ::getsockname(fd, reinterpret_cast<sockaddr *>(&saddr), &saddr_len);
    if (res == -1)
        throw_error(errno, "getsockname()");
    return ipv4_endpoint{saddr.sin_port, saddr.sin_addr.s_addr};
}
connection acceptor::accept(std::function<void()> eoc)
{
    sockaddr_in clt_addr;
    socklen_t clt_len = sizeof(clt_addr);
    int check = ::accept(fd, (sockaddr *) &clt_addr, &clt_len);
    if (check < 0) {
        throw_error(errno, "ACCEPT()");
    }
    set_fd_flags(check,get_fd_flags(check) | O_NONBLOCK);
    LOG("Connected sock %d", check);
    return connection(check, ioEntry.getparent(), eoc);
}
