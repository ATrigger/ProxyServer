//
// Created by kamenev on 05.12.15.
//

#ifndef POLL_EVENT_PROXY_SERVER_H
#define POLL_EVENT_PROXY_SERVER_H


#include <memory>
#include "connection.h"
#include <cstddef>
#include "address.h"
#include "HTTP.h"
#include "acceptor.h"
#include "events.h"
#include "outstring.h"
#include "signal_fd.h"
#include "resolver.h"
#include <map>
#include <regex>
#include <queue>
#include <mutex>
#include <boost/signals2/connection.hpp>

class proxy_server
{
    constexpr static const io::timer::timer_service::clock_t::duration connectionTimeout =
#ifdef DEBUG
        std::chrono::seconds(10)
#else
    std::chrono::seconds(120)
#endif
    ;
    constexpr static const io::timer::timer_service::clock_t::duration idleTimeout =
#ifdef DEBUG
        std::chrono::seconds(15)
#else
    std::chrono::seconds(600)
#endif
    ;
    struct inbound;
    struct outbound;
    struct FirstFound
    {
        typedef bool result_type;
        template<typename InputIterator>
        result_type operator()(InputIterator aFirstObserver, InputIterator aLastObserver) const
        {
            result_type val = false;
            for (; aFirstObserver != aLastObserver && !val; ++aFirstObserver) {
                val = *aFirstObserver;
            }
            return val;
        }
    };
    struct inbound
    {
        inbound(proxy_server *parent);
        ~inbound();
        void handleread();
        void handlewrite();
        void sendBadRequest();
        void sendNotFound();
        bool onResolve(resolver::resolverNode);

    private:
        void trySend(outstring &);
        friend struct outbound;
        proxy_server *parent;
        connection socket;
        std::shared_ptr<request> requ;
        boost::signals2::connection resolverConnection;
        std::shared_ptr<outbound> assigned;
        io::timer::timer_element timer;
        std::queue<outstring> output;
        void wakeUp();
    };
    struct outbound
    {
        outbound(io::io_service &, ipv4_endpoint, inbound *);
        ~outbound();
        void handlewrite();
        void onRead();
        void onReadDiscard();
        const std::string getHost();
    private:
        void try_to_cache();
        void askMore();
        friend struct inbound;
        ipv4_endpoint remote;
        connection socket;
        io::timer::timer_element timer;
        inbound *assigned;
        std::shared_ptr<response> resp;
        std::string host;
        std::string URI;
        std::queue<outstring> output;
        proxy_server *parent;
        bool cacheHit = false;
        bool validateRequest = false;
    };
public:
    proxy_server(io::io_service &ep, ipv4_endpoint const &local_endpoint);
    proxy_server(io::io_service &ep, ipv4_endpoint const &local_endpoint, size_t);
    ~proxy_server();
    ipv4_endpoint local_endpoint() const;
    resolver &getResolver();
    void cacheDomain(std::string &, ipv4_endpoint &);
    events resolveEvent;
    signal_fd sigfd;
    io::io_service *ios;
private:
    void on_new_connection();
    friend struct inbound;
    friend struct outbound;
    acceptor ss;
    resolver domainResolver;
    bool stop = false;
    std::map<inbound *, std::unique_ptr<inbound>> connections;
    cache::lru_cache<std::string, response> proxycache;
    boost::signals2::signal<bool(resolver::resolverNode), FirstFound> distribution;
};


#endif //POLL_EVENT_PROXY_SERVER_H
