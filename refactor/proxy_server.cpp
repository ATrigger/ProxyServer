#include <netdb.h>
#include <boost/bind.hpp>
#include <thread>
#include "HTTP.h"
#include "proxy_server.h"
#include "debug.h"

constexpr const io::timer::timer_service::clock_t::duration proxy_server::connectionTimeout;

constexpr const io::timer::timer_service::clock_t::duration proxy_server::idleTimeout;

proxy_server::inbound::inbound(proxy_server *parent)
    : parent(parent), timer(parent->batya->getClock(), proxy_server::idleTimeout, [this]
{
    LOG("Sock(inbound) %d timed out. Disconnecting", this->socket.getFd());
    this->socket.forceDisconnect();
}),
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
                  if (error != 0)
                      LOG("error = %s\n", strerror(error));
              }
              if (assigned){
                  INFO("Disconnecting assigned socket");
                  assigned->socket.forceDisconnect();}
              this->parent->connections.erase(this);
          }))
{
    socket.setOn_read(std::bind(&inbound::handleread, this));

}

void proxy_server::inbound::handleread()
{
    int n = socket.get_available_bytes();
    LOG("(%d):Bytes available: %d ", socket.getFd(), n);
    if (n < 1) {
        LOG("(%d):No bytes available. EOF.", socket.getFd());
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
    timer.recharge(proxy_server::idleTimeout);
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
        //std::cout << requ->get_request_text() << std::endl;
        parent->getResolver().sendDomainForResolve(requ->get_host());
        this->resolverConnection =
            parent->distribution.connect(
                [this](resolver::resolverNode in)
                { return this->onResolve(in); });
        //socket.setOn_read(connection::callback());
    }
}
void proxy_server::inbound::sendBadRequest()
{
    output.push(HTTP::placeholder());
    socket.setOn_read(std::bind(&inbound::handleread, this));
    socket.setOn_write(std::bind(&inbound::handlewrite, this));
}
void proxy_server::inbound::sendNotFound()
{
    output.push(HTTP::notFound());
    socket.setOn_write(std::bind(&inbound::handlewrite, this));
    socket.setOn_read(std::bind(&inbound::handleread, this));
}

void proxy_server::inbound::handlewrite()
{
    if (!output.empty()) {
        timer.recharge(proxy_server::idleTimeout);
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
    }
}

proxy_server::proxy_server(io::io_service &ep, ipv4_endpoint const &local_endpoint)
    : ss{ep, local_endpoint, std::bind(&proxy_server::on_new_connection, this)},
      sigfd{ep, [this](signalfd_siginfo)
      {
          INFO("Catched SIGINT or SIGTERM");
          this->stop = true;
      }, {SIGINT, SIGTERM}},
      resolveEvent(ep,true, [this](uint32_t)
      {
          std::unique_lock<std::mutex> distributionLock(domainResolver.getDistributeMutex());
          auto target = domainResolver.getFirst();
          distributionLock.unlock();
          distribution(target);
      })
    ,domainResolver(resolveEvent,5)
{
    batya = &ep;
    ep.setCallback([this]()
                   {
                       static int counter =0;
                       counter ++;
                       if(counter %10==0) LOG("Now connected: %lu",this->connections.size());
                       return (stop && this->connections.size() == 0)
                              ? (1) // we can now exit
                              : (0); // clients in progress.
                   });
}

ipv4_endpoint proxy_server::local_endpoint() const
{
    return ss.local_endpoint();
}

void proxy_server::on_new_connection()
{
    if (this->stop) {
        INFO("New connection after signal received. Proceeding disconnection.");
        auto tempsocket = ss.accept([]()
                                    { });
        tempsocket.forceDisconnect();
        return;
    }
    std::unique_ptr<inbound> cc(new inbound(this));
    inbound *pcc = cc.get();
    connections.emplace(pcc, std::move(cc));
}
proxy_server::outbound::outbound(io::io_service &service, ipv4_endpoint endpoint, inbound *ass)
    :
    remote(endpoint), timer(service.getClock(), proxy_server::connectionTimeout, [this]()
{
    LOG("(%d): Connection timeout.", socket.getFd());
    assigned->sendNotFound();
    socket.forceDisconnect();
}), socket(connection::connect(service, endpoint, [&]()
{
    LOG("Disconnected from (%d):%s", socket.getFd(), remote.to_string().c_str());
    if (socket.get_available_bytes() != 0) {
        LOG("(%d): Disconnected with available BYTES!!!", socket.getFd());
    }
    int error = 0;
    socklen_t errlen = sizeof(error);
    if (getsockopt(this->socket.getFd(),
                   SOL_SOCKET,
                   SO_ERROR,
                   (void *) &error,
                   &errlen) == 0) {

        if (error != 0) {
            assigned->sendBadRequest();
            LOG("error = %s\n", strerror(error));
        }
    }
    assigned->assigned.reset();
}))
{
    assigned = ass;
    output.push(ass->requ->get_request_text());
    socket.setOn_write(std::bind(&outbound::handlewrite, this));

}
bool proxy_server::inbound::onResolve(resolver::resolverNode result)
{
    if (result.host != requ->get_host()) return false;
    this->resolverConnection.disconnect();
    if (!result.ok) {
        sendNotFound();
    }
    else {
        parent->cacheDomain(result.host, result.resolvedHost);
        assigned = std::make_shared<outbound>(*parent->batya, result.resolvedHost, this);
        requ.reset();
    }
    return true;
}
proxy_server::~proxy_server()
{
}
void proxy_server::outbound::onRead()
{
    int n = socket.get_available_bytes();
    LOG("Bytes available: %d ", n);
    char buf[n + 1];
    ssize_t res = socket.read_over_connection(buf, n);
    if (res == -1) {
        throw_error(errno, "onRead()");

    }
    if (res == 0) // EOF
    {
        socket.setOn_read(connection::callback());
        socket.setOn_write(connection::callback());
        return;
    }
    buf[n] = '\0';
    LOG("(%d): response:", socket.getFd());
    //std::cout << buf << std::endl;
    outstring out(std::string(buf, n));
    out += assigned->socket.write_over_connection(out.get(), out.size());
    if (!out) {
        assigned->output.push(out);
        assigned->socket.setOn_write(std::bind(&inbound::handlewrite, assigned));
    }
}
void proxy_server::outbound::handlewrite()
{
    if (!output.empty()) {
        timer.turnOff();
        auto string = &output.front();
        size_t written = socket.write_over_connection(string->get(), string->size());
        LOG("(%d):Written:\r\n", socket.getFd());
        //std::cout << string->text.substr(string->pp, string->pp + written) << std::endl;
        string->operator+=(written);
        if (*string) {
            output.pop();
            LOG("(%d):Written all", socket.getFd());
        }
    }
    if (output.empty()) {
        LOG("(%d):Now waiting for response", socket.getFd());
        socket.setOn_read(std::bind(&outbound::onRead, this));
        socket.setOn_write(connection::callback());
    }
}
proxy_server::inbound::~inbound()
{
    if (resolverConnection.connected()) {
        resolverConnection.disconnect();
    }
}
events &proxy_server::getResolveEvent()
{
    return resolveEvent;
}
resolver &proxy_server::getResolver()
{
    return domainResolver;
}
void proxy_server::cacheDomain(std::string &string, ipv4_endpoint &endpoint)
{
    domainResolver.cacheDomain(string, endpoint);
}
