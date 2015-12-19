//
// Created by kamenev on 24.11.15.
//

#ifndef POLL_EVENT_CLIENT_H
#define POLL_EVENT_CLIENT_H

#include <string>
#include <memory>
#include <sys/epoll.h>
#include <unordered_map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
struct io_service;
struct io_element;
class connection
{
public:
    connection(std::string hostname,std::string port,
           std::unordered_map<std::string, std::function<void(io_element *, epoll_event)>> func, io_service &pe);

    ~connection() { }

    std::shared_ptr<io_element> getpoll();

private:
    std::shared_ptr<io_element> element;
};


#endif //POLL_EVENT_CLIENT_H
