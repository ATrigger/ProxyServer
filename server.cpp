//
// Created by kamenev on 23.11.15.
//

#include "server.h"
#include "io_element.h"

acceptor::acceptor(int port, int con_queue, std::unordered_map<std::string, CALLBACK() > func, io_service &pe) {

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in svr_addr, clt_addr;
    memset(&svr_addr, 0, sizeof(svr_addr));
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = htons(INADDR_ANY);
    svr_addr.sin_port = htons(port);
    if(bind(sock, (struct sockaddr *) &svr_addr, sizeof(svr_addr))<0) {exit(0);};
    listen(sock, con_queue);
    fcntl(sock, F_SETFL, O_NONBLOCK);
    this->element = std::make_shared<io_element>(sock, ACCEPT_CB, EPOLLIN, &pe, func);
}

std::shared_ptr<io_element> acceptor::getpoll() { return element; }