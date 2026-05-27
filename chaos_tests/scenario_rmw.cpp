#include "client.h"

#include <thread>
#include <vector>
#include <iostream>
#include <random>
#include <mutex>
#include <unordered_map>
#include <atomic>

static const int NUM_CLIENTS = 4;
static const int NUM_KEYS = 10;
static const int TXNS_PER_CLIENT = 500;

static std::mutex log_mutex;

struct OpRecord
{
    std::vector<std::string> keys_read;
    std::string key_written;
};

static std::vector<OpRecord> global_log;

// ---------------- worker ----------------

void worker(int id)
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

            int k1 = key_dist(rng);
            int k2 = key_dist(rng);
            int k3 = key_dist(rng);

            std::string v1, v2;

            if (client.get("k" + std::to_string(k1), v1).find("ERROR") != std::string::npos ||
                client.get("k" + std::to_string(k2), v2).find("ERROR") != std::string::npos)
            {
                client.abort();
                continue;
            }

            int a = v1.empty() ? 0 : std::stoi(v1);
            int b = v2.empty() ? 0 : std::stoi(v2);

            int sum = a + b;

            if (client.put("k" + std::to_string(k3), std::to_string(sum))
                    .find("ERROR") != std::string::npos)
            {
                client.abort();
                continue;
            }

            std::string commit_resp = client.commit();

            if (commit_resp.find("ERROR") != std::string::npos)
            {
                continue; // retry txn
            }

            {
                std::lock_guard<std::mutex> lock(log_mutex);
                global_log.push_back({{"k" + std::to_string(k1), "k" + std::to_string(k2)},
                                      "k" + std::to_string(k3)});
            }

            break;
        }
    }
}

// ---------------- reference model ----------------

static std::unordered_map<std::string, int> runReferenceModel()
{
    std::unordered_map<std::string, int> db;

    for (auto &op : global_log)
    {
        std::string a = op.keys_read[0];
        std::string b = op.keys_read[1];
        std::string c = op.key_written;

        int v1 = db[a];
        int v2 = db[b];

        db[c] = v1 + v2;
    }

    return db;
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

    // init DB
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

    auto actual = getRealState();
    auto reference = runReferenceModel();

    bool ok = true;

    for (int i = 0; i < NUM_KEYS; i++)
    {
        std::string key = "k" + std::to_string(i);

        if (reference[key] != actual[key])
        {
            std::cout << "[FAIL] " << key
                      << " ref=" << reference[key]
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