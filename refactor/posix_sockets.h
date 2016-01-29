//
// Created by kamenev on 03.12.15.
//

#ifndef POLL_EVENT_POSIX_SOCKETS_H
#define POLL_EVENT_POSIX_SOCKETS_H

#include <stdint.h>

int make_socket(int domain, int type);
void connect_socket(int fd, uint16_t port_net, uint32_t addr_net);
void start_listen(handle &fd);
void bind_socket(handle &fd, uint16_t port_net, uint32_t addr_net);
int get_fd_flags(handle &fd);
void set_fd_flags(handle &fd, int flags);
void write(handle &fd, std::string const &str);
ssize_t read_some(handle &fd, void *data, size_t size);
size_t write_some(handle &fd, void const *data, std::size_t size);
void write_all(handle &fdc, const char *data, std::size_t size);
#endif //POLL_EVENT_POSIX_SOCKETS_H
