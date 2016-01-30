//
// Created by kamenev on 06.12.15.
//

#include <thread>
#include "io_service.h"
#include "proxy_server.h"
int main()
{
    io::io_service ep;
    signal_fd ignore(ep,[](signalfd_siginfo){},{SIGPIPE});
    proxy_server proxyServer(ep, ipv4_endpoint(8080, ipv4_address::any()),10);

    ipv4_endpoint echo_server_endpoint = proxyServer.local_endpoint();
    std::cout << "bound to " << echo_server_endpoint << std::endl;

    ep.run();
    return 0;
}