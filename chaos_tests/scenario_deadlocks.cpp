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

static const int NUM_CLIENTS = 16;
static const int NUM_KEYS = 20;
static const int TEST_DURATION_SEC = 5;
static const int DEADLOCK_TIMEOUT_MS = 8000;

// ---------------- REFERENCE MODEL ----------------

static ReferenceModel reference;
static std::mutex ref_mutex;

// ---------------- WORKER ----------------

static void worker(int id, std::atomic<bool> &running)
{
    Client client("127.0.0.1", 7000);

    std::mt19937 rng(id * 991 + 123);
    std::uniform_int_distribution<int> key_dist(0, NUM_KEYS - 1);

    while (running.load())
    {
        int a = key_dist(rng);
        int b = key_dist(rng);
        int c = key_dist(rng);

        std::string k1 = "k" + std::to_string(a);
        std::string k2 = "k" + std::to_string(b);
        std::string k3 = "k" + std::to_string(c);

        auto start = std::chrono::steady_clock::now();

        bool committed = false;

        while (running.load())
        {
            if (client.begin().find("ERROR") != std::string::npos)
                continue;

            std::string v1, v2;

            if (client.get(k1, v1).find("ERROR") != std::string::npos ||
                client.get(k2, v2).find("ERROR") != std::string::npos)
            {
                client.abort();
                continue;
            }

            int x = v1.empty() ? 0 : std::stoi(v1);
            int y = v2.empty() ? 0 : std::stoi(v2);

            if (client.put(k3, std::to_string(x + y)).find("ERROR") != std::string::npos)
            {
                client.abort();
                continue;
            }

            std::string commitResp = client.commit();

            if (commitResp.find("committed") != std::string::npos)
            {
                committed = true;

                // ---------------- record in reference model ----------------
                std::lock_guard<std::mutex> lock(ref_mutex);

                static std::atomic<uint64_t> commit_order{0};
                uint64_t order = ++commit_order;

                reference.beginTxn(order);
                reference.addPut(order, k3, std::to_string(x + y));
                reference.commitTxn(order, order);
            }

            break;
        }

        auto end = std::chrono::steady_clock::now();

        int duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (duration > DEADLOCK_TIMEOUT_MS * 2)
        {
            std::cout << "[FAIL] txn blocked too long: "
                      << duration << " ms\n";
        }
    }
}

// ---------------- FETCH REAL STATE ----------------

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

// ---------------- SCENARIO ----------------

bool runScenarioDeadlockStorm()
{
    std::cout << "[scenario 6] deadlock storm test starting...\n";

    // init DB
    Client init("127.0.0.1", 7000);

    init.begin();
    for (int i = 0; i < NUM_KEYS; i++)
        init.put("k" + std::to_string(i), "1");
    init.commit();

    std::atomic<bool> running(true);
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_CLIENTS; i++)
        threads.emplace_back(worker, i, std::ref(running));

    std::this_thread::sleep_for(std::chrono::seconds(TEST_DURATION_SEC));
    running.store(false);

    for (auto &t : threads)
        t.join();

    // ---------------- VALIDATION ----------------

    reference.replayCommitted();
    auto expected = reference.state();
    auto actual = getRealState();

    bool ok = true;

    for (int i = 0; i < NUM_KEYS; i++)
    {
        std::string k = "k" + std::to_string(i);

        std::string exp = expected.count(k) ? expected[k] : "0";
        std::string act = actual.count(k) ? std::to_string(actual[k]) : "0";

        if (exp != act)
        {
            std::cout << "[FAIL] " << k
                      << " expected=" << exp
                      << " actual=" << act << "\n";
            ok = false;
        }
    }

    if (ok)
        std::cout << "[scenario 6] PASSED\n";
    else
        std::cout << "[scenario 6] FAILED\n";

    return ok;
}

bool runScenarioDeadlock()
{
    return runScenarioDeadlockStorm();
}