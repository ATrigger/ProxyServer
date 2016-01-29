//
// Created by kamenev on 03.12.15.
//

#ifndef POLL_EVENT_SERVER_H
#define POLL_EVENT_SERVER_H

#include <unordered_map>
#include "address.h"
#include "connection.h"
#include "io_service.h"
class acceptor
{
public:
    acceptor(io::io_service &, ipv4_endpoint const &,std::function<void ()>);
    ipv4_endpoint local_endpoint() const;

    handle getFd() const
    {
        return fd;
    }
    connection accept(std::function<void()> eoc);
private:
    handle fd;
    std::function<void ()> accept_connection;
    io::io_entry ioEntry;
    friend class io::io_service;
};

#endif //POLL_EVENT_SERVER_H
