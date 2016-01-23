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
    if (dnsCache.exists(string)) {
        LOG("DNS hit: %s(%s)",string.c_str(),dnsCache.get(string).to_string().c_str());
        resolverFinished.push({string, dnsCache.get(string), true});
        finisher->add();
    }
    else {
        domains.push(new std::string(string));
        newTask.notify_one();
    }
    return;
}
resolver::resolver(events &events1, size_t t)
    : finisher(&events1), dnsCache(500)
{
    for (auto i = 0; i < t; i++) resolvers.create_thread(boost::bind(&resolver::worker, this));
    // TODO: if thread creation failed we should notify
    // all previously created threads that they should quit
    // and join them
}
void resolver::worker()
{
    {
        while (true) {

            std::unique_lock<std::mutex> lk(resolveMutex);

            newTask.wait(lk, [this]()
            { return !this->domains.empty() || this->destroyThreads; });
            if (destroyThreads) return;
            std::string *domain;
            std::string port, name, input;
            this->domains.pop(domain);

            // TODO: check if this domain is already cached

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
                finisher->add();
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
                finisher->add(1);
                continue;
            }
            LOG("Looks like i got IP: %s for %s", buffer, input.c_str());
            uint16_t portShort;
            distribution.lock();
            if (!str_to_uint16(port.c_str(), &portShort)) {
                LOG("Invalid port(%s). Signal proceed",port.c_str());
                resolverFinished.push({input, {}, false});
            }
            else {
                resolverFinished.push({input, {portShort, ipv4_address(std::string(buffer))},
                                       true}/*make_pair(input, ipv4_address(std::string(buffer)))*/);

                // TODO: insert result into dnsCache and remove cacheDomain from public interface
                // TODO: we should protect accesses to dnsCache with distributeMutex
            }
            finisher->add(1);
        }
    }
}
resolver::~resolver()
{
    destroyThreads = true;
    newTask.notify_all();
    resolvers.join_all();
}
void resolver::cacheDomain(std::string &string, ipv4_endpoint &endpoint)
{
    dnsCache.put(string, endpoint);
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
    destroyThreads=true;
    newTask.notify_all();
    resolvers.join_all();
    destroyThreads=false;
    for (auto i = 0; i < t; i++) resolvers.create_thread(boost::bind(&resolver::worker, this));
}
size_t resolver::cacheSize() const
{
    return dnsCache.size();
}
