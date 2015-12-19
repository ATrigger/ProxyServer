#include <netdb.h>
#include <boost/bind.hpp>
#include <thread>
#include "HTTP.h"
#include "proxy_server.h"
#include "debug.h"

proxy_server::inbound::inbound(proxy_server *parent)
    : parent(parent), socket(parent->ss.accept([this]
                                               {
                                                   LOG("Disconnected sock %d", this->socket.getFd());
                                                   int error = 0;
                                                   socklen_t errlen = sizeof(error);
                                                   if (getsockopt(this->socket.getFd(),
                                                                  SOL_SOCKET,
                                                                  SO_ERROR,
                                                                  (void *) &error,
                                                                  &errlen) == 0) {
                                                       LOG("error = %s\n", strerror(error));
                                                   }
                                                   this->parent->connections.erase(this);
                                               }))
{
    socket.setOn_read(std::bind(&inbound::handleread, this));
    socket.setOn_write(std::bind(&inbound::handlewrite, this));
}

void proxy_server::inbound::handleread()
{
    int n = socket.get_available_bytes();
    LOG("Bytes available: %d ", n);
    if(n < 1) {
        INFO("No bytes available, yet EPOLLIN came. Disconnected");
        socket.forceDisconnect();
    }
    char buff[n+1];
    auto res = socket.read_over_connection(buff, n);
    buff[n]='\0';
    if(res == -1){
        socket.forceDisconnect();
        throw_error(errno,"Inbound::Handeread()");
    }
    if(!requ){
        requ = std::make_shared<request>(std::string(buff));
    }
    else {
        requ->add_part(std::string(buff));
    }
    std::cout<<requ->get_request_text()<<std::endl;
/*
    while (true) {
        ssize_t res = socket.read_over_connection(buf, 1500);
        if (res == -1 && errno != EWOULDBLOCK) throw_error(errno, "inbound::read()");
        else if (res == -1 && request.length() != 0) { break; }
        else if (res == -1) { return; }
        else if (res == 0) {
            INFO("REPORTED EOF");
            socket.forceDisconnect();
            return;
        }
        buf[res] = '\0';
        request += std::string(buf);
    }
    socket.setOn_read(connection::callback());
    size_t border = request.find("\r\n\r\n");
    header = request.substr(0, border + 4);
    body = request.substr(border + 4);
    request.clear();
    auto type = header.substr(0, 4);
    if (type.find("POST") == header.npos && type.find("GET") == header.npos) return sendBadRequest();
    parsedHeader = HTTP::parse(header);
    sendDomainForResolve(parsedHeader["Host"]);
    //this->resolverConnection = parent->resolver.connect(boost::bind(&inbound::resolveFinished,this,_1));
    this->resolverConnection = parent->resolver.connect([this](std::string d, ipv4_address adr)
                                                        { return this->resolveFinished(d, adr); });
    auto absolute = header.find(parsedHeader["Host"]);
    if (absolute != header.npos) {
        auto spc = header.find(' ') + 1;
        std::cout << header << std::endl;
        header = (header.substr(0, spc) + header.substr(absolute + parsedHeader["Host"].length()));
        std::cout << header << std::endl;
    }
    //std::cout << request << std::endl;
    //std::cout << header << std::endl << body << std::endl;*/
}
void proxy_server::inbound::sendBadRequest()
{
    output.push(HTTP::placeholder());
    socket.setOn_read(std::bind(&inbound::handleread, this));
    socket.setOn_write(std::bind(&inbound::handlewrite, this));
}
void proxy_server::inbound::sendDomainForResolve(std::string string)
{
  /*  auto p = string.find(':');
    if (p != string.npos) {
        host = string.substr(0, p);
        port = string.substr(p + 1);
    }
    else {
        host = string;
        port = "80";
    }
    if (parent->dnsCache.count(host) != 0) {
        parent->resolverFinished.push(make_pair(host, parent->dnsCache[host].address()));
        parent->resolveEvent.add();
    }
    else {
        parent->domains.push(new std::string(host));
    }
    parent->newTask.notify_one();
    return;*/
}

void proxy_server::inbound::handlewrite()
{
    if (!output.empty()) {
        auto string = &output.front();
        size_t written = socket.write_over_connection(string->get(),string->size());
        string->operator+=(written);
        if(*string)
            output.pop();
    }
}

proxy_server::proxy_server(io::io_service &ep, ipv4_endpoint const &local_endpoint)
    : ss{ep, local_endpoint, std::bind(&proxy_server::on_new_connection, this)},
      resolveEvent(ep, [this](uint32_t)
      {
          std::unique_lock<std::mutex> distribution(distributeMutex, std::try_to_lock_t());
          Resolvequeue copy;
          std::swap(copy, resolverFinished);
          distribution.unlock();
          while (!copy.empty()) {
              resolver(copy.front().first, copy.front().second);
              copy.pop();
          }
      })
{
    batya = &ep;
    for (int i = 0; i < 5; i++) {
        resolvers.create_thread(
            [this]()
            {
                while (true) {

                    boost::unique_lock<boost::mutex> lk(resolveMutex);

                    newTask.wait(lk, [this]()
                    { return !this->domains.empty() || this->destroyThreads; });
                    if (destroyThreads) return;
                    std::string *domain;
                    std::string port, dom;
                    this->domains.pop(domain);
                    dom = *domain;
                    port = "80";
                    delete domain;
                    struct addrinfo *r, hints;
                    bzero(&hints, sizeof(hints));
                    hints.ai_family = AF_INET;
                    hints.ai_socktype = SOCK_STREAM;
                    std::unique_lock<std::mutex> distribution(distributeMutex, std::defer_lock_t());
                    int res = getaddrinfo(dom.data(), port.data(), &hints, &r);
                    if (res != 0) {

                        LOG("Resolve failed:%s(%s). Signal proceed.",
                            dom.data(),
                            gai_strerror(res));
                        distribution.lock();
                        resolverFinished.push(make_pair(dom, ipv4_address()));
                        resolveEvent.add(1);
                        continue;
                    }
                    char buffer[INET6_ADDRSTRLEN];
                    res = getnameinfo(r->ai_addr,
                                      r->ai_addrlen,
                                      buffer,
                                      sizeof(buffer),
                                      0,
                                      0,
                                      NI_NUMERICHOST);
                    freeaddrinfo(r);
                    if (res != 0) {
                        LOG("Cannot transform to IP: %d Signal proceed", res);
                        distribution.lock();
                        resolverFinished.push(make_pair(dom, ipv4_address()));
                        resolveEvent.add(1);
                        continue;
                    }
                    LOG("Looks like i got IP: %s for %s", buffer, dom.c_str());

                    distribution.lock();
                    resolverFinished.push(make_pair(dom, ipv4_address(std::string(buffer))));
                    resolveEvent.add(1);
                }
            });
    }
}

ipv4_endpoint proxy_server::local_endpoint() const
{
    return ss.local_endpoint();
}

void proxy_server::on_new_connection()
{
    std::unique_ptr<inbound> cc(new inbound(this));
    inbound *pcc = cc.get();
    connections.emplace(pcc, std::move(cc));
}
proxy_server::outbound::outbound(io::io_service &service, ipv4_endpoint endpoint, inbound *ass)
    :
    remote(endpoint.port(), endpoint.address()), socket(connection::connect(service, endpoint, [&]()
{
    LOG("Disconnected from (%d):%s", socket.getFd(), remote.to_string().c_str());
    int error = 0;
    socklen_t errlen = sizeof(error);
    if (getsockopt(this->socket.getFd(),
                   SOL_SOCKET,
                   SO_ERROR,
                   (void *) &error,
                   &errlen) == 0) {
        LOG("error = %s\n", strerror(error));
    }
    assigned->assigned.reset();
}))
{
    assigned = ass;
//    response = ass->header + ass->body;
    socket.setOn_write(std::bind(&outbound::handlewrite, this));
}
bool proxy_server::inbound::resolveFinished(std::string domain, ipv4_address result)
{
   // if (domain != host) return false;
    INFO("eventfd success");
    this->resolverConnection.disconnect();
    if (result.address_network() == 0) {
        output.push(HTTP::notFound());
        socket.setOn_write(std::bind(&inbound::handlewrite, this));
    }
    else {
        //uint16_t port = static_cast<uint16_t>(atoi(this->port.c_str()));
      //  auto remote = ipv4_endpoint(port, result);
       // parent->dnsCache[domain] = remote;
      //  assigned = std::make_shared<outbound>(*parent->batya, remote, this);
    }
    return true;
}
proxy_server::~proxy_server()
{
    destroyThreads = true;
    newTask.notify_all();
    resolvers.join_all();
}
void proxy_server::outbound::handleread()
{
    int n = socket.get_available_bytes();
    char buf[n];
//    while (true) {
//        ssize_t res = socket.read_over_connection(buf, 200);
//        if (res == -1) {
//            assigned->socket.setOn_read(std::bind(&inbound::handleread, assigned));
//            break;
//        }
//        buf[res] = '\0';
//        assigned->socket.write_over_connection(buf, res);
//        if (res == 0) {
//            socket.forceDisconnect();
//            INFO("Reported EOF");
//            assigned->socket.setOn_read(std::bind(&inbound::handleread, assigned));
//            return;
//        }
//        //response += std::string(buf);
//    }
    ssize_t res = socket.read_over_connection(buf, n);
    if (res == -1) {
        socket.forceDisconnect();
        LOG("failed %d", errno);
    }
    if (res == 0) {
        INFO("Done reading answer");
        socket.forceDisconnect();
        return;
    }
    assigned->output.push(std::string(buf));
////    std::cout << response << std::endl;
//    assigned->request = response;
//    assigned->socket.setOn_write(std::bind(&inbound::handlewrite, assigned));
//    socket.setOn_read(std::bind(&outbound::handleread,this));
}
void proxy_server::outbound::handlewrite()
{
    LOG("Successfully connected to %s", remote.to_string().c_str());
/*    std::cout << response << std::endl;
    socket.write_all_over_connection(response.data(), response.size());
    socket.setOn_write(connection::callback());
    socket.setOn_read(std::bind(&outbound::handleread, this));
    response.clear();*/
}