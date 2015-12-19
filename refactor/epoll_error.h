#ifndef POLL_EVENT_EPOLL_ERROR_H
#define POLL_EVENT_EPOLL_ERROR_H


void throw_error [[noreturn]] (int err, char const* action);

#endif //POLL_EVENT_EPOLL_ERROR_H
