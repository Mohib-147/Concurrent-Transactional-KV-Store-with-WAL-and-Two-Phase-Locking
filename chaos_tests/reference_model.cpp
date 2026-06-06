#include "reference_model.h"
#include <algorithm>

// ---------------- txn management ----------------

void ReferenceModel::beginTxn(TxnId id)
{
    txns_[id] = Transaction{};
}

void ReferenceModel::addPut(TxnId id, const Key &key, const Value &value)
{
    txns_[id].ops.push_back({OpType::PUT, key, value});
}

void ReferenceModel::addDelete(TxnId id, const Key &key)
{
    txns_[id].ops.push_back({OpType::DELETE, key, ""});
}

void ReferenceModel::commitTxn(TxnId id, uint64_t commit_order)
{
    txns_[id].committed = true;
    txns_[id].commit_order = commit_order;
}

void ReferenceModel::abortTxn(TxnId id)
{
    txns_[id].committed = false;
}

// ---------------- execution ----------------

void ReferenceModel::replayCommitted()
{
    db_.clear();

    std::vector<std::pair<TxnId, uint64_t>> order;

    for (auto &p : txns_)
    {
        if (p.second.committed)
        {
            order.push_back({p.first, p.second.commit_order});
        }
    }

    std::sort(order.begin(), order.end(),
              [](auto &a, auto &b)
              {
                  return a.second < b.second;
              });

    for (auto &entry : order)
    {
        auto &txn = txns_[entry.first];

        for (auto &op : txn.ops)
        {
            if (op.type == OpType::PUT)
            {
                db_[op.key] = op.value;
            }
            else if (op.type == OpType::DELETE)
            {
                db_.erase(op.key);
            }
        }
    }
}

// ---------------- access ----------------

const std::unordered_map<Key, Value> &ReferenceModel::state() const
{
    return db_;
}

void ReferenceModel::clear()
{
    txns_.clear();
    db_.clear();
}