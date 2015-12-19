//
// Created by kamenev on 24.11.15.
//


#include <string.h>
#include <fcntl.h>
#include "client.h"
#include "io_element.h"

connection::connection(std::string hostname, std::string port,
               std::unordered_map<std::string, std::function<void(io_element *, epoll_event)>> func,
               io_service &pe) {
    struct addrinfo *r, hints;

    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int res = getaddrinfo("ya.ru", "80", &hints, &r);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    connect(sockfd,r->ai_addr, r->ai_addrlen);

    this->element = std::make_shared<io_element>(sockfd, CONNECT_CB, EPOLLOUT|EPOLLIN|EPOLLHUP|EPOLLRDHUP, &pe, func);
}

std::shared_ptr<io_element> connection::getpoll() {
    return this->element;
}
