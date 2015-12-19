//
// Created by kamenev on 05.12.15.
//
#include <iostream>

#include "io_service.h"
#include "echo_server.h"

int main()
{
    io::io_service ep;
    echo_server echo_server(ep, ipv4_endpoint(0, ipv4_address::any()));

    ipv4_endpoint echo_server_endpoint = echo_server.local_endpoint();
    std::cout << "bound to " << echo_server_endpoint << std::endl;

    ep.run();

    return 0;
}

