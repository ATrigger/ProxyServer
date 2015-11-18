#include<stdio.h>
#include<stdlib.h>
#include<sys/epoll.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include <signal.h>

#include<sys/socket.h>
#include<netinet/in.h>
#include "poll.h"
#include "debug.h"
static const int buf_size = 1024;

void write_cb(io_element_t * elem, struct epoll_event ev)
{

    char buf[buf_size];
    char format[128];
    sprintf(buf,"HTTP/1.1 200 OK\\r\\nConnection: Close\r\n\r\n\0");
    int val = write(elem->fd,buf,strlen(buf));
    if(val > 0){
        io_element_t *p;
        elem->remove_flag(EPOLLOUT);
        printf("send data: %d\n",elem->fd);
        elem->close_callback(elem,ev);
        elem->io_element_delete();
    }
}
void read_cb (io_element_t * elem, struct epoll_event ev)
{
    // NOTE -> read is also invoked on accept and connect
    INFO("in read_cb");
    // we just read data and print
    char buf[buf_size];
    int val = read(elem->fd, buf, buf_size);
    if (val >0)
    {
        elem->add_flag(EPOLLOUT);
        buf[val] = '\0';
        printf(" received data -> %s\n", buf);
    }
}


void close_cb (io_element_t * elem, struct epoll_event ev)
{
    INFO("in close_cb");
    printf(" disconnected %d\n",elem->fd);
    // close the socket, we are done with it
    poll_event_remove(elem->io, elem->fd);
}

void accept_cb(io_element_t * elem, struct epoll_event ev)
{
    INFO("in accept_cb");
    // accept the connection 
    struct sockaddr_in clt_addr;
    socklen_t clt_len = sizeof(clt_addr);
    int listenfd = accept(elem->fd, (struct sockaddr*) &clt_addr, &clt_len);
    fcntl(listenfd, F_SETFL, O_NONBLOCK);
    fprintf(stderr, "got the socket %d\n", listenfd);
    // set flags to check 
    uint32_t flags = EPOLLIN | EPOLLRDHUP | EPOLLHUP ;
    io_element_t *p=new io_element(listenfd,flags,elem->io);
    // add file descriptor to poll event
    poll_event_add(elem->io,listenfd,p);
    // set function callbacks 
    p->read_callback = read_cb;
    p->write_callback = write_cb;
    p->close_callback = close_cb;
}

//time out function 
int timeout_cb (io_service_t *poll_event)
{
    // just keep a count
    if (!poll_event->data)
    {
        // no count initialised, then initialize it
        INFO("init timeout counter");
        poll_event->data=calloc(1,sizeof(int));
    }
    else
    {
        // increment and print the count
        int * value = (int*)poll_event->data;
        *value+=1;
        LOG("time out number %d", *value);
        printf("tick (%d)\n", *value);

    }
    return 0;
}

int main()
{
    // create a TCP socket, bind and listen
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in svr_addr, clt_addr;
    memset(&svr_addr, 0 , sizeof(svr_addr));
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = htons(INADDR_ANY);
    svr_addr.sin_port = htons(8080);
    bind(sock, (struct sockaddr *) &svr_addr, sizeof(svr_addr));
    listen(sock, 10);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    // create a poll event object, with time out of 1 sec
    io_service_t pe(1000);
    // set timeout callback
    pe.timeout_callback = timeout_cb;
    io_element_t *p = new io_element(sock,EPOLLIN,&pe);
    // add sock to poll event
    poll_event_add(&pe, sock, p);
    // set callbacks
    //p->read_callback = read_cb;
    p->accept_callback = accept_cb;
    p->close_callback = close_cb;
    // enable accept callback
//    p->cb_flags |= ACCEPT_CB;
    p->sc_flag(ACCEPT_CB);
    // start the event loop
    io_start(pe);

    return 0;
}

