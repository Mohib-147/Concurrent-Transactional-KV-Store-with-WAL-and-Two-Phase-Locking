#ifndef STORE_H
#define STORE_H

#include "kvdb.h"
#include <map>
#include <shared_mutex>

class Store
{
public:
    Store();
    ~Store();

    bool get(const Key &key, Value &out_value) const;

    void put(const Key &key, const Value &value);

    bool delete_key(const Key &key);

    bool exists(const Key &key) const;

    std::vector<Key> getAllKeys() const;

    size_t size() const;

    void clear();

private:
    std::map<Key, Value> data_;
};

#endif