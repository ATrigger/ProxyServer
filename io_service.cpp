#ifndef _IOSERVICE_CPP
#define _IOSERVICE_CPP

#include "io_service.h"
#include "io_element.h"
#include "server.h"
#include "debug.h"

io_service::io_service(size_t timeout, std::function<int(io_service *)> func) {
    this->table = hashmap();
    this->timeout_callback = func;
    this->data = NULL;
    this->timeout = timeout;
    this->epoll_fd = epoll_create(MAX_EVENTS);
    INFO("Created a new poll event");

}


void poll_event_delete(io_service *poll_event) {
    INFO("deleting a io_service");
    close(poll_event->epoll_fd);
    delete poll_event;
}

int io_service::addOrUpdate_event(int fd, io_element *poll_element) {
    io_element *elem = NULL;
    std::lock_guard<std::mutex> lcg(tM);
    elem = (io_element *) table[fd];

    if (elem) {
        lcg.~lock_guard();
        LOG("fd (%d) already added updating flags", fd);
        struct epoll_event ev;
        memset(&ev, 0, sizeof(struct epoll_event));
        ev.data.fd = fd;
        ev.events = poll_element->events;
        return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    }
    else {
        table[fd] = poll_element;
        lcg.~lock_guard();
        LOG("Added fd(%d)", fd);
        struct epoll_event ev;
        memset(&ev, 0, sizeof(struct epoll_event));
        ev.data.fd = poll_element->fd;
        ev.events = poll_element->events;
        return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    }
}


int io_service::remove_event(int fd) {
    std::lock_guard<std::mutex> lcg(tM);
    table.erase(fd);
    lcg.~lock_guard();
    close(fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    return 0;
}


int io_service::process() {
    struct epoll_event events[MAX_EVENTS];

    int fds = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout);
    if (fds == 0) {
        //INFO("event loop timed out");
        if (timeout_callback) {
            if (timeout_callback(this)) {
                //Если возвращается что-то кроме 0 -> всем начинаем писать закрытие соединений
                for (auto elem:table) {
                    elem.second->add_flag(EPOLLOUT);
                }

                //return -1;
            }
        }
    }
    int i = 0;
    for (; i < fds; i++) {
        io_element *value = NULL;

        if (table.count(events[i].data.fd) != 0 && (value = table[events[i].data.fd]) != NULL) {
            LOG("started processing for event id(%d) and sock(%d)", i, events[i].data.fd);
            // when data avaliable for read or urgent flag is set
            if ((events[i].events & EPOLLIN) || (events[i].events & EPOLLPRI)) {
                if (events[i].events & EPOLLIN) {
                    LOG("found EPOLLIN for event id(%d) and sock(%d)", i, events[i].data.fd);
                    value->cur_event &= EPOLLIN;
                }
                else {
                    LOG("found EPOLLPRI for event id(%d) and sock(%d)", i, events[i].data.fd);
                    value->cur_event &= EPOLLPRI;
                }
                /// connect or accept callbacks also go through EPOLLIN
                /// accept callback if flag set
                if ((value->cb_flags & ACCEPT_CB) && (value->accept_callback))
                    value->accept_callback(value, events[i]);
                /// connect callback if flag set
                if ((value->cb_flags & CONNECT_CB) && (value->connect_callback))
                    value->connect_callback(value, events[i]);
                /// read callback in any case
                if (value->read_callback)
                    value->read_callback(value, events[i]);
            }
            // when write possible
            if (events[i].events & EPOLLOUT) {
                LOG("found EPOLLOUT for event id(%d) and sock(%d)", i, events[i].data.fd);
                value->cur_event &= EPOLLOUT;
                if (value->write_callback)
                    value->write_callback(value, events[i]);
            }
            // shutdown or error
            if ((events[i].events & EPOLLRDHUP) || (events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                if (events[i].events & EPOLLRDHUP) {
                    LOG("found EPOLLRDHUP for event id(%d) and sock(%d)", i, events[i].data.fd);
                    value->cur_event &= EPOLLRDHUP;
                }
                else {
                    LOG("found EPOLLERR for event id(%d) and sock(%d)", i, events[i].data.fd);
                    value->cur_event &= EPOLLERR;
                }
                if (value->close_callback)
                    value->close_callback(value, events[i]);
            }
        }
        else {
            LOG("WARNING: NOT FOUND hash table value for event id(%d) and sock(%d)", i, events[i].data.fd);
        }
    }
    return 0;
}

void io_service::loop() {
    INFO("Entering the main event loop for epoll lib");
    while (!process());
}

void io_service::setTimeoutCallback(std::function<int(io_service *)> func) {
    this->timeout_callback = func;
}


#endif