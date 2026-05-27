#include "client.h"

#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <chrono>

// ---------------- CONFIG ----------------

static const int NUM_CLIENTS = 8;
static const int TXNS_PER_CLIENT = 500;
static const std::string KEY_PREFIX = "k";

// ---------------- GLOBAL COMMIT LOG ----------------

struct CommitRecord
{
    std::string key;
    std::string value;
};

static std::mutex log_mutex;
static std::vector<CommitRecord> committed_transactions;

// ---------------- WORKER ----------------

void worker(int id, std::atomic<bool> &running)
{
    Client client("127.0.0.1", 7000);

    std::mt19937 rng(id * 999 + 42);
    std::uniform_int_distribution<int> val_dist(1, 100000);
    std::uniform_int_distribution<int> key_dist(0, 9);

    for (int i = 0; i < TXNS_PER_CLIENT; i++)
    {
        if (!running.load())
            break;

        while (true)
        {
            std::string beginResp = client.begin();
            if (beginResp.find("ERROR") != std::string::npos)
                continue;

            int k = key_dist(rng);
            std::string key = KEY_PREFIX + std::to_string(k);
            std::string value = std::to_string(val_dist(rng));

            std::string putResp = client.put(key, value);
            if (putResp.find("ERROR") != std::string::npos)
            {
                client.abort();
                continue;
            }

            std::string commitResp = client.commit();

            // ONLY commit success matters
            if (commitResp.find("ERROR") == std::string::npos)
            {
                std::lock_guard<std::mutex> lock(log_mutex);
                committed_transactions.push_back({key, value});
            }

            break;
        }
    }
}

// ---------------- FETCH REAL STATE ----------------

std::unordered_map<std::string, std::string> fetchDB()
{
    Client c("127.0.0.1", 7000);

    std::unordered_map<std::string, std::string> db;

    for (int i = 0; i < 10; i++)
    {
        std::string key = KEY_PREFIX + std::to_string(i);

        std::string val;
        c.begin();
        c.get(key, val);
        c.commit();

        db[key] = val;
    }

    return db;
}

// ---------------- VERIFICATION ----------------

bool verifyCrash()
{
    std::cout << "[scenario 5] verifying crash recovery...\n";

    auto db = fetchDB();

    std::unordered_map<std::string, std::vector<std::string>> expected_values;

    {
        std::lock_guard<std::mutex> lock(log_mutex);

        for (auto &r : committed_transactions)
        {
            expected_values[r.key].push_back(r.value);
        }
    }

    bool ok = true;

    // Check:
    // final value must be one of committed writes per key
    for (auto &kv : db)
    {
        const std::string &key = kv.first;
        const std::string &actual = kv.second;

        auto &valid_vals = expected_values[key];

        if (valid_vals.empty())
        {
            std::cout << "[FAIL] no committed writes for " << key << "\n";
            ok = false;
            continue;
        }

        bool found = false;

        for (auto &v : valid_vals)
        {
            if (v == actual)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            std::cout << "[FAIL] key=" << key
                      << " unexpected value=" << actual << "\n";
            ok = false;
        }
    }

    if (ok)
        std::cout << "[scenario 5] PASSED\n";
    else
        std::cout << "[scenario 5] FAILED\n";

    return ok;
}

// ---------------- SCENARIO RUNNER ----------------

bool runScenarioCrash()
{
    std::cout << "[scenario 5] starting crash-under-load test...\n";

    std::atomic<bool> running(true);

    std::vector<std::thread> threads;

    // 8 clients
    for (int i = 0; i < NUM_CLIENTS; i++)
        threads.emplace_back(worker, i, std::ref(running));

    // let system run for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "[scenario 5] sending CRASH command...\n";

    Client crashClient("127.0.0.1", 7000);
    crashClient.sendCommand("CRASH");

    // stop workers
    running.store(false);

    for (auto &t : threads)
        t.join();

    std::cout << "[scenario 5] server crashed, now restart manually and rerun test\n";

    return true;
}