#include <netdb.h>
#include <boost/bind.hpp>
#include <thread>
#include "HTTP.h"
#include "proxy_server.h"
#include "debug.h"

proxy_server::inbound::inbound(proxy_server *parent)
    : parent(parent),
      socket(parent->ss.accept(
          [this]
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
              if (assigned) assigned->socket.forceDisconnect();
              this->parent->connections.erase(this);
          }))
{
    socket.setOn_read(std::bind(&inbound::handleread, this));

}

void proxy_server::inbound::handleread()
{
    int n = socket.get_available_bytes();
    LOG("Bytes available: %d ", n);
    if (n < 1) {
        INFO("No bytes available, yet EPOLLIN came. Disconnected");
        socket.forceDisconnect();
        return;
    }
    char buff[n + 1];
    auto res = socket.read_over_connection(buff, n);
    buff[n] = '\0';
    if (res == -1) {
        throw_error(errno, "Inbound::Handeread()");
    }
    if (res == 0) {
        socket.forceDisconnect();
        return;
    }
    if (!requ) {
        requ = std::make_shared<request>(std::string(buff));
    }
    else {
        requ->add_part(std::string(buff));
    }
    if (requ->get_state() == request::FAIL) {
        sendBadRequest();
    }
    if (requ->get_state() == request::BODYFULL) {
        std::cout << requ->get_request_text() << std::endl;
        sendDomainForResolve(requ->get_host());
        //socket.setOn_read(connection::callback());
    }
}
void proxy_server::inbound::sendBadRequest()
{
    output.push(HTTP::placeholder());
    socket.setOn_read(std::bind(&inbound::handleread, this));
    socket.setOn_write(std::bind(&inbound::handlewrite, this));
}
void proxy_server::inbound::sendDomainForResolve(std::string string)
{
    if (parent->dnsCache.count(string) != 0) {
        parent->resolverFinished.push({string, parent->dnsCache[string], true});
        parent->resolveEvent.add();
    }
    else {
        parent->domains.push(new std::string(string));
        parent->newTask.notify_one();
    }
    this->resolverConnection =
        parent->resolver.connect(
            [this](resolverNode in)
            { return this->resolveFinished(in); });
    return;
}

void proxy_server::inbound::handlewrite()
{
    if (!output.empty()) {
        auto string = &output.front();
        size_t written = socket.write_over_connection(string->get(), string->size());
        string->operator+=(written);
        if (*string) {
            output.pop();
            INFO("Written all");
        }
    }
    if (output.empty()) {
        socket.setOn_write(connection::callback());
        //socket.setOn_read(std::bind(&inbound::handleread, this));
    }
}

proxy_server::proxy_server(io::io_service &ep, ipv4_endpoint const &local_endpoint)
    : ss{ep, local_endpoint, std::bind(&proxy_server::on_new_connection, this)},
      resolveEvent(ep, [this](uint32_t)
      {
          std::unique_lock<std::mutex> distribution(distributeMutex, std::try_to_lock_t());
          resolveQueue_t copy;
          std::swap(copy, resolverFinished);
          distribution.unlock();
          while (!copy.empty()) {
              resolver(copy.front());
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
                    std::string port, name, input;
                    this->domains.pop(domain);
                    input = *domain;
                    name = input;
                    port = "80";
                    delete domain;
                    auto it = input.find(':');
                    if (it != input.npos) {
                        port = input.substr(it + 1);
                        name = input.substr(0, it);
                    }
                    struct addrinfo *r, hints;
                    bzero(&hints, sizeof(hints));
                    hints.ai_family = AF_INET;
                    hints.ai_socktype = SOCK_STREAM;
                    std::unique_lock<std::mutex> distribution(distributeMutex, std::defer_lock_t());
                    int res = getaddrinfo(name.data(), port.data(), &hints, &r);
                    if (res != 0) {

                        LOG("Resolve failed:%s(%s:%s)(%s). Signal proceed.",
                            input.data(), name.data(), port.data(),
                            gai_strerror(res));
                        distribution.lock();
                        resolverFinished.push({input, {}, false});
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
                        resolverFinished.push({input, {}, false});
                        resolveEvent.add(1);
                        continue;
                    }
                    LOG("Looks like i got IP: %s for %s", buffer, input.c_str());

                    distribution.lock();
                    resolverFinished.push({input, {atoi(port.c_str()), ipv4_address(std::string(buffer))},
                                           true}/*make_pair(input, ipv4_address(std::string(buffer)))*/);
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
    remote(endpoint), socket(connection::connect(service, endpoint, [&]()
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
        if (error != 0) assigned->sendBadRequest();
    }
    assigned->assigned.reset();
}))
{
    assigned = ass;
    output.push(ass->requ->get_request_text());
    socket.setOn_write(std::bind(&outbound::handlewrite, this));
}
bool proxy_server::inbound::resolveFinished(resolverNode result)
{
    if (result.host != requ->get_host()) return false;
    this->resolverConnection.disconnect();
    if (!result.ok) {
        output.push(HTTP::notFound());
        socket.setOn_write(std::bind(&inbound::handlewrite, this));
        socket.setOn_read(std::bind(&inbound::handleread, this));
    }
    else {
        parent->dnsCache[result.host] = result.resolvedHost;
        assigned = std::make_shared<outbound>(*parent->batya, result.resolvedHost, this);
        requ.reset();
    }
    return true;
}
proxy_server::~proxy_server()
{
    destroyThreads = true;
    newTask.notify_all();
    resolvers.join_all();
}
void proxy_server::outbound::onRead()
{
    int n = socket.get_available_bytes();
    LOG("Bytes available: %d ", n);
    char buf[n + 1];
    ssize_t res = socket.read_over_connection(buf, n);
    if (res == -1) {
        throw_error(errno, "onRead()");
        return;
    }
    if (res == 0) // EOF
    {
        socket.setOn_read(connection::callback());
        socket.setOn_write(connection::callback());
        return;
    }
    buf[n] = '\0';
    assigned->output.push(std::string(buf, n));
    assigned->socket.setOn_write(std::bind(&inbound::handlewrite, assigned));

}
void proxy_server::outbound::handlewrite()
{
    if (!output.empty()) {
        auto string = &output.front();
        size_t written = socket.write_over_connection(string->get(), string->size());
        string->operator+=(written);
        if (*string)
            output.pop();
    }
    if (output.empty()) {
        socket.setOn_read(std::bind(&outbound::onRead, this));
        socket.setOn_write(connection::callback());
    }
}