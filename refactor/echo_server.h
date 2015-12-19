#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include <map>
#include "address.h"
#include "io_service.h"
#include "acceptor.h"
#include "connection.h"

struct echo_server
{
    struct bind
    {
        bind(echo_server* parent);
        void update();

    private:
        echo_server* parent;
        connection socket;
        size_t start_offset;
        size_t end_offset;
        char buf[1500];
    };

    echo_server(io::io_service& ep, ipv4_endpoint const& local_endpoint);

    ipv4_endpoint local_endpoint() const;

private:
    void on_new_connection();

private:
    acceptor ss;
    std::map<bind*, std::unique_ptr<bind>> connections;
};

#endif // ECHO_SERVER_H
