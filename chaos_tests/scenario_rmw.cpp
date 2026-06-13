#include "client.h"
#include "reference_model.h"

#include <thread>
#include <vector>
#include <iostream>
#include <random>
#include <mutex>
#include <atomic>

static const int NUM_CLIENTS = 4;
static const int NUM_KEYS = 10;
static const int TXNS_PER_CLIENT = 500;

static std::mutex model_mutex;
static ReferenceModel ref;
static std::atomic<uint64_t> commit_counter{0};

// ---------------- worker ----------------

static void worker(int id)
{
    Client client("127.0.0.1", 7000);

    std::mt19937 rng(id * 999 + 42);
    std::uniform_int_distribution<int> key_dist(0, NUM_KEYS - 1);

    for (int i = 0; i < TXNS_PER_CLIENT; i++)
    {
        while (true)
        {
            std::string resp = client.begin();
            if (resp.find("ERROR") != std::string::npos)
                continue;

            TxnId txn_id = id * 100000 + i; // unique txn id

            ref.beginTxn(txn_id);

            int k1 = key_dist(rng);
            int k2 = key_dist(rng);
            int k3 = key_dist(rng);

            std::string v1, v2;

            std::string r1 = client.get("k" + std::to_string(k1), v1);
            std::string r2 = client.get("k" + std::to_string(k2), v2);

            if (r1.find("ERROR") != std::string::npos ||
                r2.find("ERROR") != std::string::npos)
            {
                client.abort();
                ref.abortTxn(txn_id);
                continue;
            }

            int a = v1.empty() ? 0 : std::stoi(v1);
            int b = v2.empty() ? 0 : std::stoi(v2);

            int sum = a + b;

            std::string wkey = "k" + std::to_string(k3);
            std::string wval = std::to_string(sum);

            std::string p = client.put(wkey, wval);

            if (p.find("ERROR") != std::string::npos)
            {
                client.abort();
                ref.abortTxn(txn_id);
                continue;
            }

            ref.addPut(txn_id, wkey, wval);

            std::string commit_resp = client.commit();

            if (commit_resp.find("ERROR") != std::string::npos)
            {
                ref.abortTxn(txn_id);
                continue;
            }

            uint64_t order = commit_counter.fetch_add(1);
            ref.commitTxn(txn_id, order);

            break;
        }
    }
}

// ---------------- real state fetch ----------------

static std::unordered_map<std::string, int> getRealState()
{
    Client c("127.0.0.1", 7000);

    std::unordered_map<std::string, int> db;

    for (int i = 0; i < NUM_KEYS; i++)
    {
        std::string val;

        c.begin();
        c.get("k" + std::to_string(i), val);
        c.commit();

        db["k" + std::to_string(i)] =
            val.empty() ? 0 : std::stoi(val);
    }

    return db;
}

// ---------------- scenario runner ----------------

bool runScenarioRMW()
{
    std::cout << "[scenario 2] RMW contention test starting...\n";

    Client init("127.0.0.1", 7000);

    init.begin();
    for (int i = 0; i < NUM_KEYS; i++)
        init.put("k" + std::to_string(i), "1");
    init.commit();

    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_CLIENTS; i++)
        threads.emplace_back(worker, i);

    for (auto &t : threads)
        t.join();

    // build reference DB
    ref.replayCommitted();
    auto reference = ref.state();
    auto actual = getRealState();

    bool ok = true;

    for (int i = 0; i < NUM_KEYS; i++)
    {
        std::string key = "k" + std::to_string(i);

        int ref_val = 0;

        if (reference.find(key) != reference.end() &&
            !reference.at(key).empty())
        {
            ref_val = std::stoi(reference.at(key));
        }

        if (ref_val != actual[key])
        {
            std::cout << "[FAIL] " << key
                      << " ref=" << ref_val
                      << " actual=" << actual[key] << "\n";

            ok = false;
        }
    }

    if (ok)
        std::cout << "[scenario 2] PASSED\n";
    else
        std::cout << "[scenario 2] FAILED\n";

    return ok;
}