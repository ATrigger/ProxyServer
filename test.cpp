#include<stdio.h>
#include<stdlib.h>
#include<sys/epoll.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include <signal.h>

#include<sys/socket.h>
#include<netinet/in.h>
#include <iostream>
#include "io_element.h"
#include "debug.h"
#include "io_service.h"
#include "server.h"
#include "HTTP.h"
#include "client.h"


static const int buf_size = 2048;

void write_cb(io_element *elem, struct epoll_event ev) {

    int val = write(elem->fd, elem->data.rqOrs.c_str(), elem->data.rqOrs.length());
    if (val > 0) {
        elem->remove_flag(EPOLLOUT);
        elem->data.rqOrs.clear();
        printf("send data: %d\n", elem->fd);
//        elem->close_callback(elem, ev);
//        elem->io_element_delete();
    }
}

void connected_cb(io_element *elem, struct epoll_event ev) {
    INFO("CONNECTED");
    //std::pair<sockaddr*, socklen_t > *data = (std::pair<sockaddr*, socklen_t > *)elem->data.anydata;

    //delete data;

}

void read_client_cb(io_element *elem, struct epoll_event ev) {
    INFO("READ AS CLIENT");
}

void write_client_cb(io_element *elem, struct epoll_event ev) {
    INFO("WRITE AS CLIENT");
    int val = send(elem->fd,elem->data.rqOrs.c_str(),elem->data.rqOrs.size(),0);
    if(val >0){
        printf("send request: %d\n", elem->fd);

    }
    else {
        INFO("SEND FAILED");
        LOG("%d",errno);

    }
    elem->remove_flag(EPOLLOUT);
}
void close_cb(io_element *elem, struct epoll_event ev) {
    INFO("in close_cb");
    printf(" disconnected %d\n", elem->fd);

    elem->io->remove_event(elem->fd);
}
void read_cb(io_element *elem, struct epoll_event ev) {

    INFO("in read_cb");

    char buf[buf_size];
    ssize_t val;
    if ((val = read(elem->fd, buf, buf_size)) != -1) {

        buf[val] = '\0';
        elem->data.rqOrs.append(buf);
        printf(" received data -> %s\n", buf);
    }
    if(strlen(buf)==0) return;

  /*  elem->data.parsed = HTTP::parse(elem->data.rqOrs);
    std::string host = elem->data.parsed["Host"];
    auto x = host.find(':');
    std::string hostname = host.substr(0, x);
    std::string port = host.substr(x+1);
    if (std::regex_match(hostname, HTTP::isIp)) {
        //Тред не нужон
        connection in(hostname, port, {{"connect", connected_cb},
                                             {"read",    read_client_cb},
                                             {"write",   write_client_cb},{"close",close_cb}}, *elem->io);
        elem->data.relative=in.getpoll();
        elem->data.relative->data.parsed =elem->data.parsed;
        elem->data.relative->data.relative=(std::shared_ptr<io_element>(elem));
        elem->io->add_event(in);
    }
    else if (!elem->io->dns.count(host)) {
        //нужен тред
    }
    else {
        //Тред не нужон.
    }*/
    elem->add_flag(EPOLLOUT);
}




void accept_cb(io_element *elem, struct epoll_event ev) {
    INFO("in accept_cb");

    struct sockaddr_in clt_addr;
    socklen_t clt_len = sizeof(clt_addr);
    int listenfd = accept(elem->fd, (struct sockaddr *) &clt_addr, &clt_len);
    fcntl(listenfd, F_SETFL, O_NONBLOCK);
    fprintf(stderr, "got the socket %d\n", listenfd);

    uint32_t flags = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
    io_element *p = new io_element(listenfd, 0, flags, elem->io, std::unordered_map<std::string, CALLBACK() >
            {{"write", write_cb},
             {"read",  read_cb},
             {"close", close_cb}});

    elem->io->addOrUpdate_event(listenfd, p);


}


int timeout_cb(io_service *poll_event) {

    if (!poll_event->data) {

        INFO("init timeout counter");
        poll_event->data = calloc(1, sizeof(int));
    }
    else {

        int *value = (int *) poll_event->data;
        *value += 1;
        //LOG("time out number %d", *value);
        //printf("tick (%d)\n", *value);

    }
    return 0;
}

int main() {

    io_service pe(1000, timeout_cb);
    acceptor ss(8080, 10, std::unordered_map<std::string, CALLBACK() >
            {{"accept", accept_cb},
             {"close",  close_cb}}, pe);

    pe.add_event(ss);
    io_start(pe);

    return 0;
}

