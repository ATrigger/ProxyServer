//
// Created by kamenev on 23.11.15.
//

#ifndef POLL_EVENT_SERVER_H
#define POLL_EVENT_SERVER_H
/*std::unordered_map<std::string, CALLBACK() >
{{"accept", accept_cb},
{"close",  close_cb}}*/

#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <memory>
#include <sys/epoll.h>
#include <unordered_map>

struct io_service;
struct io_element;

class acceptor {
public:
    acceptor(int port, int con_queue,
           std::unordered_map<std::string, std::function<void(io_element *, epoll_event)>> func, io_service &pe);

    ~acceptor() { }

    std::shared_ptr<io_element> getpoll();

private:
    std::shared_ptr<io_element> element;
};


#endif //POLL_EVENT_SERVER_H
