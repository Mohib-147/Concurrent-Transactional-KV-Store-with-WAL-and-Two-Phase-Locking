#include "store.h"
#include <iostream>

Store::Store() {}

Store::~Store() {}

bool Store::get(const Key &key, Value &out_value) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end())
    {
        out_value = it->second;
        return true;
    }
    return false;
}

void Store::put(const Key &key, const Value &value)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    data_[key] = value;
}

bool Store::delete_key(const Key &key)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return data_.erase(key) > 0;
}

bool Store::exists(const Key &key) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return data_.find(key) != data_.end();
}

std::vector<Key> Store::getAllKeys() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<Key> keys;
    for (const auto &pair : data_)
    {
        keys.push_back(pair.first);
    }
    return keys;
}

size_t Store::size() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return data_.size();
}

void Store::clear()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    data_.clear();
}
