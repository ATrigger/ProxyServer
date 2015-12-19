#include <sys/socket.h>
#include <errno.h>
#include <stdint.h>
#include <cstddef>
#include <unistd.h>
#include <netinet/in.h>
#include <string>
#include <fcntl.h>
#include "epoll_error.h"

int make_socket(int domain, int type) {
    int fd = ::socket(domain, type, 0);
    if (fd == -1)
        throw_error(errno, "socket()");

    return fd;
}

void start_listen(int fd) {
    int res = ::listen(fd, SOMAXCONN);
    if (res == -1)
        throw_error(errno, "listen()");
}

void bind_socket(int fd, uint16_t port_net, uint32_t addr_net) {
    sockaddr_in saddr{};
    int optval =1;
    setsockopt(fd,SOL_SOCKET,SO_REUSEPORT,&optval,sizeof(optval));
    saddr.sin_family = AF_INET;
    saddr.sin_port = port_net;
    saddr.sin_addr.s_addr = addr_net;
    int res = ::bind(fd, reinterpret_cast<sockaddr const *>(&saddr), sizeof saddr);
    if (res == -1)
        throw_error(errno, "bind()");
}

void connect_socket(int fd, uint16_t port_net, uint32_t addr_net) {
    sockaddr_in saddr{};
    saddr.sin_family = AF_INET;
    saddr.sin_port = port_net;
    saddr.sin_addr.s_addr = addr_net;

    int res = ::connect(fd, reinterpret_cast<sockaddr const *>(&saddr), sizeof saddr);
    if (res == -1 && errno != EINPROGRESS)
        throw_error(errno, "connect()");
}

int get_fd_flags(int fd) {
    int res = fcntl(fd, F_GETFL, 0);
    if (res == -1)
        throw_error(errno, "fcntl(F_GETFL)");
}

void set_fd_flags(int fd, int flags) {
    int res = fcntl(fd, F_SETFL, flags);
    if (res == -1)
        throw_error(errno, "fcntl(F_SETFL)");
}

size_t write_some(int fd, void const* data, std::size_t size)
{


    ssize_t res = write(fd, data, size);
    if (res == -1)
    {
        int err = errno;
        if (err == EAGAIN || err == ECONNRESET)
            return 0;
        throw_error(err, "write()");
    }
    return static_cast<size_t>(res);
}

void write_all(int fdc, const char *data, std::size_t size)
{

    ssize_t total_bytes_written = 0;
    while (total_bytes_written != size)
    {
        ssize_t written = write_some(fdc, &data[total_bytes_written], size-total_bytes_written);
        if(written <= 0){
            break;
        }
        total_bytes_written+=written;
    }
    if(total_bytes_written!=size){
        throw_error(errno, "write_all()");
    }
}

void write(int fd, std::string const& str)
{

    write_all(fd, str.data(), str.size());
}

ssize_t read_some(int fd, void* data, size_t size)
{
    ssize_t res = ::read(fd, data, size);
    if (res == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
        throw_error(errno, "read()");
    return res;
}

