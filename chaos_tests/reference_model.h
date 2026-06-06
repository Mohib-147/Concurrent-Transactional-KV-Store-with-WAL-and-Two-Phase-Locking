#ifndef REFERENCE_MODEL_H
#define REFERENCE_MODEL_H

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

using Key = std::string;
using Value = std::string;
using TxnId = uint32_t;

class ReferenceModel
{
public:
    enum class OpType
    {
        PUT,
        DELETE
    };

    struct Operation
    {
        OpType type;
        Key key;
        Value value;
    };

    struct Transaction
    {
        std::vector<Operation> ops;
        bool committed = false;
        uint64_t commit_order = 0; // LSN or logical ordering
    };

    // add operations
    void beginTxn(TxnId id);
    void addPut(TxnId id, const Key &key, const Value &value);
    void addDelete(TxnId id, const Key &key);

    void commitTxn(TxnId id, uint64_t commit_order);
    void abortTxn(TxnId id);

    // apply final result
    void replayCommitted();

    // comparison target
    const std::unordered_map<Key, Value> &state() const;

    void clear();

private:
    std::unordered_map<TxnId, Transaction> txns_;
    std::unordered_map<Key, Value> db_;
};

#endif