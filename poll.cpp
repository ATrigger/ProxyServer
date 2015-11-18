#include "poll.h"
#include "debug.h"
#include <unordered_map>

#include <stdlib.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include<unistd.h>

void io_element::add_flag(int flag){
    events|=flag;
    poll_event_add(io,fd,this);

}
void io_element::remove_flag(int flag){
    events&=~flag;
    poll_event_add(io,fd,this);

}

io_element::io_element(int fd, uint32_t events, io_service *io) {

    INFO("Creating a new poll event element");
    this->io = io;
    this->fd = fd;
    this->events = events;
}

void io_element::io_element_delete() {
    INFO("Deleting a poll event element");
    free(this);
}


io_service::io_service(int timeout) {
    this->table = hashmap();

    this->data = NULL;
    this->timeout = timeout;
    this->epoll_fd = epoll_create(MAX_EVENTS);
    INFO("Created a new poll event");

}


void poll_event_delete(io_service_t *poll_event) {
    INFO("deleting a io_service");
    close(poll_event->epoll_fd);
    free(poll_event);
}

int poll_event_add(io_service_t *poll_event,int fd,io_element_t *poll_element) {
    io_element_t *elem = NULL;
    elem = (io_element_t *) poll_event->table[fd];
    if (elem) {
        LOG("fd (%d) already added updating flags", fd);
        struct epoll_event ev;
        memset(&ev, 0, sizeof(struct epoll_event));
        ev.data.fd = fd;
        ev.events = poll_element->events;
        return epoll_ctl(poll_event->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    }
    else {

        poll_event->table[fd] = poll_element;
        LOG("Added fd(%d)", fd);
        struct epoll_event ev;
        memset(&ev, 0, sizeof(struct epoll_event));
        ev.data.fd = poll_element->fd;
        ev.events = poll_element->events;
        return epoll_ctl(poll_event->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    }
}


int poll_event_remove(io_service_t *poll_event, int fd) {
    poll_event->table.erase(fd);
    close(fd);
    epoll_ctl(poll_event->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    return 0;
}


int poll_event_process(io_service_t *poll_event) {
    struct epoll_event events[MAX_EVENTS];
    int fds = epoll_wait(poll_event->epoll_fd, events, MAX_EVENTS, poll_event->timeout);
    if (fds == 0) {
        INFO("event loop timed out");
        if (poll_event->timeout_callback) {
            if (poll_event->timeout_callback(poll_event)) {
                //Если возвращается что-то кроме 0 -> всем начинаем писать закрытие соединений
                for(auto elem:poll_event->table){
                    elem.second->add_flag(EPOLLOUT);
                }
                //return -1;
            }
        }
    }
    int i = 0;
    for (; i < fds; i++) {
        io_element_t *value = NULL;

        if (poll_event->table.count(events[i].data.fd) != 0 && (value = poll_event->table[events[i].data.fd]) != NULL) {
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
                    value->read_callback( value, events[i]);
            }
            // when write possible
            if (events[i].events & EPOLLOUT) {
                LOG("found EPOLLOUT for event id(%d) and sock(%d)", i, events[i].data.fd);
                value->cur_event &= EPOLLOUT;
                if (value->write_callback)
                    value->write_callback( value, events[i]);
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
                    value->close_callback( value, events[i]);
            }
        }
        else
        {
            LOG("WARNING: NOT FOUND hash table value for event id(%d) and sock(%d)", i, events[i].data.fd);
        }
    }
    return 0;
}

void poll_event_loop(io_service_t &poll_event) {
    INFO("Entering the main event loop for epoll lib");
    while (!poll_event_process(&poll_event));
}

void io_element::sc_flag(int flag) {
    this->cb_flags|=flag;
}
