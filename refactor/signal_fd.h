//
// Created by kamenev on 08.01.16.
//

#ifndef POLL_EVENT_SIGNALS_H
#define POLL_EVENT_SIGNALS_H
#include <functional>
#include <sys/signalfd.h>
#include <vector>
#include <signal.h>
#include "io_service.h"
#include "epoll_error.h"

class signal_fd
{
    typedef std::function<void (signalfd_siginfo)> callback;
    typedef uint8_t signal;
public:
    signal_fd(io::io_service&,callback,std::vector<signal>);
    void modifymask(std::vector<signal>&);
private:
    int createfd(std::vector<signal> &vector);
    callback on_ready;
    io::io_entry ioEntry;
    int fd;
    int createfd();
};


#endif //POLL_EVENT_SIGNALS_H
