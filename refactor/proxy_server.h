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
#include <map>
#include <regex>
#include <queue>
#include <boost/signals2.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <mutex>

class proxy_server
{
    constexpr static const io::timer::timer_service::clock_t::duration connectionTimeout = std::chrono::seconds(100);
    constexpr static const io::timer::timer_service::clock_t::duration idleTimeout = std::chrono::seconds(15);
    struct inbound;
    struct outbound;
    struct resolverNode
    {
        resolverNode(std::string _host, ipv4_endpoint to, bool flag): host(_host), resolvedHost(to), ok(flag){}
        std::string host;
        ipv4_endpoint resolvedHost;
        bool ok;
    };
    struct inbound
    {
        inbound(proxy_server *parent);
        ~inbound();
        void handleread();
        void handlewrite();
        void sendBadRequest();
        void sendNotFound();
        bool resolveFinished(resolverNode);
        void sendDomainForResolve(std::string);
    private:
        friend struct outbound;
        proxy_server *parent;
        connection socket;
        std::shared_ptr<request> requ;
        boost::signals2::connection resolverConnection;
        std::shared_ptr<outbound> assigned;
        io::timer::timer_element timer;
        std::queue<outstring> output;


    };
    struct outbound
    {
        outbound(io::io_service&,ipv4_endpoint,inbound *);
        void handlewrite();
        void onRead();
    private:
        friend struct inbound;
        ipv4_endpoint remote;
        connection socket;
        io::timer::timer_element timer;
        inbound* assigned;
        std::shared_ptr<response> resp;
        std::queue<outstring> output;
    };
public:
    proxy_server(io::io_service &ep, ipv4_endpoint const &local_endpoint);
    ~proxy_server();
    ipv4_endpoint local_endpoint() const;
    typedef std::queue<resolverNode> resolveQueue_t;
    struct FirstFound
    {
        typedef bool result_type;
        template <typename InputIterator> result_type operator()(InputIterator aFirstObserver, InputIterator aLastObserver) const {
            result_type val = false;
            for (; aFirstObserver != aLastObserver && !val; ++aFirstObserver)  {
                val = *aFirstObserver;
            }
            return val;
        }
    };
    boost::signals2::signal<bool (resolverNode), FirstFound> resolver;
    boost::lockfree::queue<std::string*,boost::lockfree::capacity<30>> domains;
    boost::condition_variable newTask;
    boost::mutex resolveMutex;
    std::mutex distributeMutex;
    resolveQueue_t resolverFinished;
    signal_fd sigfd;
    events resolveEvent;
    io::io_service *batya;
    boost::thread_group resolvers;
    bool destroyThreads = false;
private:
    void on_new_connection();
    friend struct inbound;
    friend struct outbound;

    acceptor ss;
    bool stop=false;
    std::map<std::string,ipv4_endpoint> dnsCache;
    std::map<inbound *, std::unique_ptr<inbound>> connections;
};



#endif //POLL_EVENT_PROXY_SERVER_H
