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
#include <boost/optional.hpp>
#include <boost/thread.hpp>
#include <mutex>
#include <bits/stl_queue.h>
#include <condition_variable>

class resolver
{
public:
    struct resolverNode
    {
        resolverNode(std::string _host, ipv4_endpoint to): host(_host), resolvedHost(to){} //OK
        resolverNode(std::string _host):host(_host){} //Resolver failed
        std::string host;
        //ipv4_endpoint resolvedHost; // TODO: replace with boost::optional<ipv4_endpoint> DONE
        boost::optional<ipv4_endpoint> resolvedHost;
    };
    typedef std::queue<resolverNode> resolveQueue_t;
    resolver(events &, size_t);
    resolverNode getFirst();
    void sendDomainForResolve(std::string);
    size_t cacheSize() const;
    void resize(size_t);
    ~resolver();
private:
    void worker();
    void stopWorkers();
    void sendToDistribution(const resolverNode &n);
    //boost::lockfree::queue<std::string*,boost::lockfree::capacity<30>> domains;
    std::queue<std::string> domains;
    std::condition_variable newTask;
    std::mutex resolveMutex;
    boost::thread_group resolvers;
    bool destroyThreads = false; // TODO: protect with mutex. DONE
    std::mutex distributeMutex;
    resolveQueue_t resolverFinished;
    cache::lru_cache<std::string,ipv4_endpoint> dnsCache;
    events *finisher;
};


#endif //POLL_EVENT_RESOLVER_H
