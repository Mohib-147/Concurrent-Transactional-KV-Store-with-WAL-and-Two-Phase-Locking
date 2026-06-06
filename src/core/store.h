#ifndef STORE_H
#define STORE_H

#include "kvdb.h"
#include <unordered_map>
#include <vector>

class Store
{
public:
    Store();
    ~Store();

    bool get(const Key &key, Value &out_value) const;

    void put(const Key &key, const Value &value);

    std::unordered_map<Key, Value> getAllEntries() const;

    bool delete_key(const Key &key);

    bool exists(const Key &key) const;

    std::vector<Key> getAllKeys() const;

    size_t size() const;

    void clear();

    struct WriteOp
    {
        Key key;
        Value value;
        bool is_delete;
    };

    void applyBatch(const std::vector<WriteOp> &ops);

private:
    std::unordered_map<Key, Value> data_;
    mutable std::mutex mutex_;
};

#endif