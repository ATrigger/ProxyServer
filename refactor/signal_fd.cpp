//
// Created by kamenev on 08.01.16.
//

#include <sys/epoll.h>
#include <unistd.h>
#include "signal_fd.h"
#include "debug.h"
#include "posix_sockets.h"
handle signal_fd::createfd(std::vector<signal> &vector)
{
    int sfd;
    sigset_t mask;
    sigemptyset(&mask);
    for (auto i:vector) {
        if (sigaddset(&mask, i) != 0) {
            LOG("Warning: %d is not a valid signal", i);
        }
    }
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        throw_error(errno, "sigprocmask()");
    }
    sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (sfd < 0) {
        throw_error(errno, "signalfd(-1)");
    }
    return handle(sfd);
}
handle signal_fd::createfd()
{
    int sfd;
    sigset_t mask;
    sigemptyset(&mask);
    sfd = signalfd(-1,&mask,SFD_CLOEXEC|SFD_NONBLOCK);
    if(sfd <0){
        throw_error(errno,"signalfd(-1,empty)");
    }
    return handle(sfd);
}
void signal_fd::modifymask(std::vector<signal> &vector)
{
    int sfd;
    sigset_t mask;
    sigemptyset(&mask);
    for (auto i:vector) {
        if (sigaddset(&mask, i) != 0) {
            LOG("Warning: %d is not a valid signal", i);
        }
    }
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        throw_error(errno, "sigprocmask()");
    }
    sfd = signalfd(fd.get_raw(), &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if(sfd < 0){
        throw_error(errno,"signalfd()");
    }
}
signal_fd::signal_fd(io::io_service &service,
                     signal_fd::callback callback,
                     std::vector<signal_fd::signal> vector)
    : on_ready(std::move(callback)),fd(createfd(vector)),
      ioEntry(service,fd,EPOLLIN,[this](uint32_t)
      {

          signalfd_siginfo sigfd;
          read_some(fd, &sigfd, sizeof(sigfd));
          this->on_ready(sigfd);
      })
{
}
