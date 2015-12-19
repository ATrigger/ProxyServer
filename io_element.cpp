#include "io_element.h"
#include "debug.h"
#include <unordered_map>

#include <stdlib.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include<unistd.h>

void io_element::add_flag(int flag) {
    events |= flag;
    io->addOrUpdate_event(fd, this);

}

void io_element::remove_flag(int flag) {
    events &= ~flag;
    io->addOrUpdate_event(fd, this);

}

io_element::io_element(int fd, int flag, uint32_t events, io_service *io,
                       std::unordered_map<std::string, CALLBACK() > func) {

    INFO("Creating a new poll event element");
    std::unordered_map<std::string, CALLBACK() *> callbacks
            {{"write",   &this->write_callback},
             {"read",    &this->read_callback},
             {"close",   &this->close_callback},
             {"accept",  &this->accept_callback},
             {"connect", &this->connect_callback}};
    this->io = io;
    this->fd = fd;
    this->events = events;
    if (flag != 0) {
        cb_flags |= flag;
    }
    for (auto &elem:func) {
        *callbacks[elem.first] = elem.second;
    }
}

void io_element::io_element_delete() {
    INFO("Deleting a poll event element");
    delete this;
}


