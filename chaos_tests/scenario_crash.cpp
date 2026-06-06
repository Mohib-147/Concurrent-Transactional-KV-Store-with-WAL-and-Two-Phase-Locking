#include "client.h"
#include "reference_model.h"

#include <thread>
#include <vector>
#include <iostream>
#include <random>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <chrono>

// ---------------- CONFIG ----------------

static const int NUM_CLIENTS = 8;
static const int TXNS_PER_CLIENT = 500;
static const int NUM_KEYS = 10;
static const std::string KEY_PREFIX = "k";

// ---------------- GLOBAL STATE ----------------

static std::mutex log_mutex;
static ReferenceModel reference;

// ---------------- WORKER ----------------

void worker(int id, std::atomic<bool> &running)
{
    Client client("127.0.0.1", 7000);

    std::mt19937 rng(id * 999 + 42);
    std::uniform_int_distribution<int> val_dist(1, 100000);
    std::uniform_int_distribution<int> key_dist(0, NUM_KEYS - 1);

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

            if (client.put(key, value).find("ERROR") != std::string::npos)
            {
                client.abort();
                continue;
            }

            std::string commitResp = client.commit();

            if (commitResp.find("ERROR") == std::string::npos)
            {
                std::lock_guard<std::mutex> lock(log_mutex);

                // assign txn id locally (just sequential log index)
                static std::atomic<uint64_t> commit_order{0};
                uint64_t order = ++commit_order;

                reference.beginTxn(order);
                reference.addPut(order, key, value);
                reference.commitTxn(order, order);
            }

            break;
        }
    }
}

// ---------------- FETCH REAL DB ----------------

std::unordered_map<std::string, std::string> fetchDB()
{
    Client c("127.0.0.1", 7000);

    std::unordered_map<std::string, std::string> db;

    for (int i = 0; i < NUM_KEYS; i++)
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

// ---------------- VERIFY ----------------

bool verifyScenarioCrash()
{
    std::cout << "[scenario 5] verifying crash recovery...\n";

    // 1. replay reference model
    reference.replayCommitted();
    auto expected = reference.state();

    // 2. fetch real DB
    auto actual = fetchDB();

    bool ok = true;

    // 3. compare full state
    for (int i = 0; i < NUM_KEYS; i++)
    {
        std::string key = KEY_PREFIX + std::to_string(i);

        std::string exp = expected.count(key) ? expected.at(key) : "";
        std::string act = actual.count(key) ? actual.at(key) : "";

        if (exp != act)
        {
            std::cout << "[FAIL] " << key
                      << " expected=" << exp
                      << " actual=" << act << "\n";
            ok = false;
        }
    }

    if (ok)
        std::cout << "[scenario 5] PASSED\n";
    else
        std::cout << "[scenario 5] FAILED\n";

    return ok;
}

// ---------------- SCENARIO RUN ----------------

bool runScenarioCrash()
{
    std::cout << "[scenario 5] starting crash-under-load test...\n";

    std::atomic<bool> running(true);
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_CLIENTS; i++)
        threads.emplace_back(worker, i, std::ref(running));

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "[scenario 5] sending CRASH...\n";

    Client crashClient("127.0.0.1", 7000);
    crashClient.sendCommand("CRASH");

    running.store(false);

    for (auto &t : threads)
        t.join();

    std::cout << "[scenario 5] server crashed. restart server then run VERIFY.\n";

    return true;
}