//
// Created by kamenev on 10.01.16.
//

#include <netdb.h>
#include <cassert>
#include <thread>
#include "resolver.h"
#include "debug.h"
#include "utils.h"
void resolver::sendDomainForResolve(std::string string)
{
//    if (dnsCache.exists(string)) {
//        LOG("DNS hit: %s(%s)", string.c_str(), dnsCache.get(string).to_string().c_str());
//        resolverFinished.push({string, dnsCache.get(string)});
//        finisher->add();
//    }
//    else {
    std::unique_lock<std::mutex> resolveLock(resolveMutex);
    domains.push(string);
    newTask.notify_one();
//    }
    return;
}
resolver::resolver(events &events1, size_t t)
    : finisher(&events1), dnsCache(500)
{
    LOG("Starting with %lu workers",t);
    try {
        for (auto i = 0; i < t; i++) resolvers.create_thread(boost::bind(&resolver::worker, this));
    }
    catch (std::exception &e) {
        stopWorkers();
        throw;
    }
    // TODO: if thread creation failed we should notify DONE
    // all previously created threads that they should quit
    // and join them
}
void resolver::worker()
{
    {
        while (true) {
            std::unique_lock<std::mutex> resolveLock(resolveMutex);
            newTask.wait(resolveLock, [this]()
            { return !this->domains.empty() || this->destroyThreads; });
            if (destroyThreads) return;
            std::string port, name, input;
            input = domains.front();
            domains.pop();
            std::unique_lock<std::mutex> distributeLock(distributeMutex);
            bool hit = dnsCache.exists(input);
            distributeLock.unlock();
            if (hit) {
                LOG("DNS hit: %s", input.c_str());
                sendToDistribution({input, dnsCache.get(input)});
                continue;
            }
            // TODO: check if this domain is already cached. DONE
            name = input;
            port = "80";
            auto it = input.find(':');
            if (it != input.npos) {
                port = input.substr(it + 1);
                name = input.substr(0, it);
            }
            struct addrinfo *r, hints;
            bzero(&hints, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            int res = getaddrinfo(name.data(), port.data(), &hints, &r);
            if (res != 0) {

                LOG("Resolve failed:%s(%s:%s)(%s). Signal proceed.",
                    input.data(), name.data(), port.data(),
                    gai_strerror(res));
                sendToDistribution({input});
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
                sendToDistribution({input});
                continue;
            }
            LOG("Looks like i got IP: %s for %s", buffer, input.c_str());
            uint16_t portShort;
            if (!str_to_uint16(port.c_str(), &portShort)) {
                LOG("Invalid port(%s). Signal proceed", port.c_str());
                sendToDistribution({input});
            }
            else {
                sendToDistribution({input, {portShort, ipv4_address(std::string(buffer))}});
                // TODO: insert result into dnsCache and remove cacheDomain from public interface DONE
                // TODO: we should protect accesses to dnsCache with distributeMutex DONE
            }
        }
    }
}
resolver::~resolver()
{
    stopWorkers();
}
void resolver::sendToDistribution(const resolverNode &n)
{
    std::unique_lock<std::mutex> distribution(distributeMutex);
    if(n.resolvedHost && !dnsCache.exists(n.host)) {
        LOG("Put in cache: %s",n.host.c_str());
        dnsCache.put(n.host,n.resolvedHost.get());
    }
    resolverFinished.push(n);
    finisher->add();
}
void resolver::stopWorkers()
{
    std::unique_lock<std::mutex> lk(resolveMutex);
    destroyThreads = true;
    lk.unlock();
    newTask.notify_all();
    resolvers.join_all();
}
resolver::resolverNode resolver::getFirst()
{
    assert(!resolverFinished.empty());
    std::unique_lock<std::mutex> distributionLock(distributeMutex);
    auto result = resolverFinished.front();
    resolverFinished.pop();
    return result;
}
void resolver::resize(size_t t)
{
    stopWorkers();
    destroyThreads = false;
    for (auto i = 0; i < t; i++) resolvers.create_thread(boost::bind(&resolver::worker, this));
    LOG("Resized to %lu workers",t);
}
size_t resolver::cacheSize() const
{
    return dnsCache.size();
}
