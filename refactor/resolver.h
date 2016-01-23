//
// Created by kamenev on 10.01.16.
//

#ifndef POLL_EVENT_RESOLVER_H
#define POLL_EVENT_RESOLVER_H


#include "address.h"
#include "lrucache.h"
#include "events.h"
#include <boost/signals2.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <mutex>
#include <bits/stl_queue.h>

class resolver
{
public:
    struct resolverNode
    {
        resolverNode(std::string _host, ipv4_endpoint to, bool flag): host(_host), resolvedHost(to), ok(flag){}
        std::string host;
        ipv4_endpoint resolvedHost;
        bool ok;
    };
    typedef std::queue<resolverNode> resolveQueue_t;
    resolver(events &, size_t);
    resolverNode getFirst();
    void sendDomainForResolve(std::string);
    void cacheDomain(std::string&,ipv4_endpoint&);
    size_t cacheSize() const;
    void resize(size_t);
    ~resolver();
private:
    void worker();
    boost::lockfree::queue<std::string*,boost::lockfree::capacity<30>> domains;
    boost::condition_variable newTask;
    boost::mutex resolveMutex;
    boost::thread_group resolvers;
    bool destroyThreads = false;
    std::mutex distributeMutex;
    resolveQueue_t resolverFinished;
    cache::lru_cache<std::string,ipv4_endpoint> dnsCache;
    events *finisher;
};


#endif //POLL_EVENT_RESOLVER_H
