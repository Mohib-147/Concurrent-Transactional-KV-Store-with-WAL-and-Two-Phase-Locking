#include "store.h"

Store::Store() {}

Store::~Store() {}

bool Store::get(const Key &key, Value &out_value) const
{
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
    data_[key] = value;
}

bool Store::delete_key(const Key &key)
{
    return data_.erase(key) > 0;
}

bool Store::exists(const Key &key) const
{
    return data_.find(key) != data_.end();
}

std::vector<Key> Store::getAllKeys() const
{
    std::vector<Key> keys;
    keys.reserve(data_.size());

    for (const auto &pair : data_)
    {
        keys.push_back(pair.first);
    }

    return keys;
}

size_t Store::size() const
{
    return data_.size();
}

void Store::clear()
{
    data_.clear();
}

void Store::applyBatch(const std::vector<WriteOp> &ops)
{
    for (const auto &op : ops)
    {
        if (op.is_delete)
        {
            data_.erase(op.key);
        }
        else
        {
            data_[op.key] = op.value;
        }
    }
}

std::unordered_map<Key, Value> Store::getAllEntries() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}