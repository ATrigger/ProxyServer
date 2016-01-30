#include <netdb.h>
#include <boost/bind.hpp>
#include <thread>
#include "HTTP.h"
#include "proxy_server.h"
#include "debug.h"

constexpr const io::timer::timer_service::clock_t::duration proxy_server::connectionTimeout;

constexpr const io::timer::timer_service::clock_t::duration proxy_server::idleTimeout;

proxy_server::inbound::inbound(proxy_server *parent)
    : parent(parent), timer(parent->ios->getClock(), proxy_server::idleTimeout, [this]
{
    LOG("Sock(inbound) %d timed out. Disconnecting", this->socket.getFd().get_raw());
    this->socket.forceDisconnect();
}),
      socket(parent->ss.accept(
          [this]
          {
              LOG("Disconnected sock %d", this->socket.getFd().get_raw());
              getSocketError(this->socket.getFd());
              if (assigned) {
                  INFO("Disconnecting assigned socket");
                  assigned->socket->forceDisconnect();
              }
              this->parent->connections.erase(this);
          }))
{
    wakeUp();
}

void proxy_server::inbound::handleread()
{
    size_t n = socket.get_available_bytes();
    LOG("(%d):Bytes available: %lu ", socket.getFd().get_raw(), n);
    if (n < 1) {
        LOG("(%d):No bytes available. EOF.", socket.getFd().get_raw());
        socket.forceDisconnect();
        return;
    }
    char buff[n + 1];
    auto res = socket.read_over_connection(buff, n);
    buff[n] = '\0';
    if (res == -1) {
        throw_error(errno, "Inbound::Handleread()");
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
    else if (requ->get_state() == request::BODYFULL) {
        parent->getResolver().sendDomainForResolve(requ->get_host());
        LOG("(%d):Sent to resolver.", socket.getFd().get_raw());
        this->resolverConnection =
            parent->distribution.connect(
                [this](resolver::resolverNode in)
                { return this->onResolve(in); });
    }
}
void proxy_server::inbound::sendBadRequest()
{
    output.push(HTTP::placeholder());
    wakeUp();
}
void proxy_server::inbound::sendNotFound()
{
    output.push(HTTP::notFound());
    wakeUp();
}
void proxy_server::inbound::wakeUp()
{
    socket.setOn_rw(std::bind(&inbound::handleread, this), std::bind(&inbound::handlewrite, this));
}
void proxy_server::inbound::handlewrite()
{
    if (!output.empty()) {
        timer.recharge(proxy_server::idleTimeout);
        auto string = &output.front();
        size_t written = socket.write_over_connection(string->get(), string->size());
        string->operator+=(written);
        LOG("(%d):Written %lu bytes to client", socket.getFd().get_raw(), written);
        if (*string) {
            output.pop();
            INFO("Written all");
        }
    }
    if (output.empty()) {
        if (assigned) assigned->askMore();
        socket.setOn_write(connection::callback());
    }
}
proxy_server::proxy_server(io::io_service &ep, ipv4_endpoint const &local_endpoint, size_t t)
    :
    proxy_server(ep, local_endpoint)
{
    domainResolver.resize(t);
}
proxy_server::proxy_server(io::io_service &ep, ipv4_endpoint const &local_endpoint)
    : ss{ep, local_endpoint, std::bind(&proxy_server::on_new_connection, this)},
      sigfd{ep, [this](signalfd_siginfo)
      {
          INFO("Catched SIGINT or SIGTERM");
          this->stop = true;
      }, {SIGINT, SIGTERM}},
      resolveEvent(ep, true, [this](uint32_t)
      {
          auto target = domainResolver.getFirst();
          distribution(target);
      }), domainResolver(resolveEvent, 5), proxycache(10000)
{
    ios = &ep;
    ep.setCallback([this]()
                   {
#ifdef DEBUG
                       static int counter = 0;
                       counter++;

                       if (counter % 10 == 0) {
                           LOG("Now connected: %lu", this->connections.size());
                           LOG("Cache entries DNS: %lu, pages: %lu",
                               this->domainResolver.cacheSize(),
                               this->proxycache.size());
                       }
#endif
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
bool proxy_server::inbound::onResolve(resolver::resolverNode result)
{
    if (result.host != requ->get_host()) return false;
    this->resolverConnection.disconnect();
    if (!result.resolvedHost) {
        sendNotFound();
    }
    else {
        if(!assigned) assigned = std::make_shared<outbound>(this);
        if(assigned->getHost()!=requ->get_host()) assigned->perform_connection(result.resolvedHost.get());
#ifdef DEBUG
        if(assigned->getHost() == requ->get_host()) INFO("FAST PATH");
#endif
        assigned->form_request();
        requ.reset();
    }
    return true;
}
proxy_server::~proxy_server()
{
}

proxy_server::outbound::outbound(inbound *ass)
    :
    assigned(ass), parent(ass->parent)
{}
void proxy_server::outbound::perform_connection(ipv4_endpoint endpoint){
    timer = io::timer::timer_element(parent->ios->getClock(),
        proxy_server::connectionTimeout,
        [this]()
        {
            LOG("(%d): Connection timeout.", socket->getFd().get_raw());
            assigned->sendNotFound();
            socket->forceDisconnect();
        });
    socket = std::unique_ptr<connection>(new connection(connection::connect(*parent->ios,endpoint,[&]()
    {
        LOG("Disconnected from (%d):%s", socket->getFd().get_raw(),host.c_str());
        if (socket->get_available_bytes() != 0) {
            LOG("(%d): Disconnected with available BYTES!!!", socket->getFd().get_raw());
        }
        if (getSocketError(this->socket->getFd()) != 0) {
            assigned->sendBadRequest();
        }
        assigned->assigned.reset();
    })));
}
void proxy_server::outbound::form_request(){
    assert(socket);
    host = assigned->requ->get_host();
    URI = assigned->requ->get_URI();
    validateRequest = assigned->requ->is_validating();
    cacheHit = parent->proxycache.exists(host + URI);
    if (!validateRequest
        && cacheHit) {
        auto cache_entry = parent->proxycache.get(host + URI);
        auto etag = cache_entry.get_header("ETag");
        LOG("Cache hit: %s", URI.c_str());
        INFO("Validating request");
        assigned->requ->append_header("If-None-Match", etag);
    }
    output.push(assigned->requ->get_request_text());
    socket->setOn_write(std::bind(&outbound::handlewrite, this));
}
void proxy_server::outbound::onRead()
{
    assert(socket);
    size_t n = socket->get_available_bytes();
    char buf[n + 1];
    ssize_t res = socket->read_over_connection(buf, n);
    if (res == -1) {
        throw_error(errno, "onRead()");
    }
    if (res == 0) // EOF
    {
        LOG("(%d):Outbound EOF. Disconnected", socket->getFd().get_raw());
        socket->forceDisconnect();
        return;
    }
    buf[n] = '\0';
    assigned->timer.recharge(proxy_server::idleTimeout);
    if (!resp) {
        resp = std::make_shared<response>(std::string(buf, n));
    }
    else {
        resp->add_part({buf, n});
    }
    socket->setOn_read(connection::callback());
    if (resp->get_state() >= HTTP::FIRSTLINE && resp->get_code() == "304" && cacheHit) {//NOT MODIFIED 304
        LOG("Cache valid (%d):(%s)", socket->getFd().get_raw(), resp->get_code().c_str());
        auto cache_entry = parent->proxycache.get(host + URI);
        outstring out(cache_entry.get_text());
        assigned->trySend(out);
        socket->setOn_read(std::bind(&outbound::onReadDiscard, this));
    }
    else {
        if (cacheHit) {
            LOG("Couldn't use cache (%d):(%s)", socket->getFd().get_raw(), resp->get_code().c_str());
            cacheHit = false; // we need to re-update cache;
        }
        outstring out(std::string(buf, n));
        assigned->trySend(out);
    }
}
void proxy_server::outbound::handlewrite()
{
    assert(socket);
    if (!output.empty()) {
        timer.turnOff(); // Connection successful. No need to check connection_timeout
        auto string = &output.front();
        size_t written = socket->write_over_connection(string->get(), string->size());
        string->operator+=(written);
        if (*string) {
            output.pop();
        }
    }
    if (output.empty()) {
        socket->setOn_rw(std::bind(&outbound::onRead, this), connection::callback());
    }
}
proxy_server::inbound::~inbound()
{
    if (resolverConnection.connected()) {
        resolverConnection.disconnect();
    }
}
resolver &proxy_server::getResolver()
{
    return domainResolver;
}
void proxy_server::outbound::try_to_cache()
{
    if (resp && resp->is_cacheable() && !cacheHit) {
        std::string temp = host + URI;
        LOG("Cached: %s (%s)", temp.c_str(), resp->get_header("ETag").c_str());
        parent->proxycache.put(temp, response(*resp));
    }

}
void proxy_server::outbound::onReadDiscard()
{
    assert(socket);
    size_t n = socket->get_available_bytes();
    LOG("Bytes discarded: %lu ", n);
    char buf[n + 1];
    ssize_t res = socket->read_over_connection(buf, n);
    if (res == -1) {
        throw_error(errno, "NestedRead()");
    }
}
void proxy_server::outbound::askMore()
{
    assert(socket);
    if (!cacheHit) {
        socket->setOn_read(std::bind(&outbound::onRead, this));
    }
}
void proxy_server::inbound::trySend(outstring &out)
{
    out += socket.write_over_connection(out.get(), out.size());
    if (!out) {
        output.push(out);
        socket.setOn_write(std::bind(&inbound::handlewrite, this));
    }
    else if (assigned) assigned->askMore();
}
proxy_server::outbound::~outbound()
{
    try_to_cache();
}
const std::string proxy_server::outbound::getHost()
{
    return host;
}
