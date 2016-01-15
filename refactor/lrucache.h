//
// Created by kamenev on 10.01.16.
//

#ifndef POLL_EVENT_LRUCACHE_H
#define POLL_EVENT_LRUCACHE_H

#include <stddef.h>
#include <list>
#include <unordered_map>
namespace cache
{

template<typename key_t, typename value_t>
class lru_cache
{
public:
    typedef typename std::pair<key_t, value_t> key_value_pair_t;

    lru_cache(size_t max_size)
        :_max_size(max_size){}
    ~lru_cache(){}

    void remove(const key_t &key){
        auto it = _cache_items_map.find(key);
        if (it != _cache_items_map.end()) {
            _cache_items_list.erase(it->second);
            _cache_items_map.erase(it);
        }
    }
    void put(const key_t &key, const value_t &value)
    {
        auto it = _cache_items_map.find(key);
        if (it != _cache_items_map.end()) {
            _cache_items_list.erase(it->second);
            _cache_items_map.erase(it);
        }

        _cache_items_list.push_front(key_value_pair_t(key, value));
        _cache_items_map[key] = _cache_items_list.begin();

        if (_cache_items_map.size() > _max_size) {
            auto last = _cache_items_list.end();
            last--;
            _cache_items_map.erase(last->first);
            _cache_items_list.pop_back();
        }
    }

    const value_t &get(const key_t &key)
    {
        auto it = _cache_items_map.find(key);
        if (it == _cache_items_map.end()) {
            throw std::range_error("Key not found");
        }
        else {
            _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
            return it->second->second;
        }
    }

    bool exists(const key_t &key) const
    {
        return _cache_items_map.find(key) != _cache_items_map.end();
    }

    size_t size() const
    {

        return _cache_items_map.size();
    }

//private:
    std::list<key_value_pair_t> _cache_items_list;
    std::unordered_map<key_t, decltype(_cache_items_list.begin())> _cache_items_map;
    size_t _max_size;
};
}

#endif //POLL_EVENT_LRUCACHE_H
