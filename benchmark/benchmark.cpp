#include "../chaos_tests/client.h"

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <mutex>
#include <unordered_map>

// ============================================================
// CONFIG
// ============================================================

static const std::string HOST = "127.0.0.1";
static const int PORT = 7000;

static const int NUM_CLIENTS = 32;
static const int SINGLE_CLIENT_TXNS = 10000;
static const int MULTI_CLIENT_TXNS = 1000;

static const int WORKING_SET = 1000;

static std::mutex log_mutex;

// ============================================================
// TIMING HELPERS
// ============================================================

using Clock = std::chrono::steady_clock;

double durationSec(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;
}

// ============================================================
// BENCHMARK 1: SINGLE CLIENT
// ============================================================

void benchmark_single_client()
{
    std::cout << "\n=== BENCHMARK 1: Single Client ===\n";

    Client client(HOST, PORT);

    auto start = Clock::now();

    for (int i = 0; i < SINGLE_CLIENT_TXNS; i++)
    {
        client.begin();
        client.put("k1", std::to_string(i));
        client.commit();
    }

    auto end = Clock::now();

    double sec = durationSec(start, end);
    double tps = SINGLE_CLIENT_TXNS / sec;

    std::cout << "Transactions: " << SINGLE_CLIENT_TXNS << "\n";
    std::cout << "Time: " << sec << " sec\n";
    std::cout << "TPS: " << tps << "\n";
}

// ============================================================
// WORKER FOR CONCURRENT TESTS
// ============================================================

void worker(int id,
            std::atomic<int> &counter,
            std::atomic<int> &abort_count,
            int total_txns)
{
    Client client(HOST, PORT);

    std::mt19937 rng(id * 999 + 42);
    std::uniform_int_distribution<int> key_dist(0, WORKING_SET - 1);

    while (true)
    {
        int current = counter.fetch_add(1);
        if (current >= total_txns)
            break;

        std::string key = "k" + std::to_string(key_dist(rng));
        std::string val = std::to_string(current);

        client.begin();

        if (client.put(key, val).find("ERROR") != std::string::npos)
        {
            client.abort();
            abort_count++;
            continue;
        }

        std::string resp = client.commit();

        if (resp.find("ERROR") != std::string::npos)
        {
            abort_count++;
        }
    }
}

// ============================================================
// BENCHMARK 2: NO GROUP COMMIT
// ============================================================

void benchmark_no_group_commit()
{
    std::cout << "\n=== BENCHMARK 2: 32 Clients (group commit OFF) ===\n";

    std::atomic<int> counter(0);
    std::atomic<int> aborts(0);

    auto start = Clock::now();

    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_CLIENTS; i++)
        threads.emplace_back(worker, i, std::ref(counter), std::ref(aborts), NUM_CLIENTS * MULTI_CLIENT_TXNS);

    for (auto &t : threads)
        t.join();

    auto end = Clock::now();

    double sec = durationSec(start, end);
    double total = NUM_CLIENTS * MULTI_CLIENT_TXNS;

    std::cout << "TPS: " << total / sec << "\n";
    std::cout << "Abort rate: " << (double)aborts / total << "\n";
}

// ============================================================
// BENCHMARK 3: GROUP COMMIT
// ============================================================

void benchmark_group_commit()
{
    std::cout << "\n=== BENCHMARK 3: 32 Clients (group commit ON) ===\n";

    std::atomic<int> counter(0);
    std::atomic<int> aborts(0);

    auto start = Clock::now();

    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_CLIENTS; i++)
        threads.emplace_back(worker, i, std::ref(counter), std::ref(aborts), NUM_CLIENTS * MULTI_CLIENT_TXNS);

    for (auto &t : threads)
        t.join();

    auto end = Clock::now();

    double sec = durationSec(start, end);
    double total = NUM_CLIENTS * MULTI_CLIENT_TXNS;

    std::cout << "TPS: " << total / sec << "\n";
    std::cout << "Abort rate: " << (double)aborts / total << "\n";
}

// ============================================================
// BENCHMARK 4: READ HEAVY WORKLOAD
// ============================================================

void read_worker(int id,
                 std::atomic<int> &counter,
                 int total,
                 bool repeatable_read)
{
    Client client(HOST, PORT);

    std::mt19937 rng(id * 123 + 7);
    std::uniform_int_distribution<int> key_dist(0, WORKING_SET - 1);

    while (true)
    {
        int c = counter.fetch_add(1);
        if (c >= total)
            break;

        std::string key = "k" + std::to_string(key_dist(rng));
        std::string val;

        client.begin(repeatable_read ? "REPEATABLE_READ" : "READ_COMMITTED");

        client.get(key, val);

        if (c % 20 == 0)
        {
            client.put(key, std::to_string(c));
        }

        client.commit();
    }
}

void benchmark_read_heavy()
{
    std::cout << "\n=== BENCHMARK 4: Read Heavy ===\n";

    for (bool rr : {false, true})
    {
        std::atomic<int> counter(0);

        auto start = Clock::now();

        std::vector<std::thread> threads;

        for (int i = 0; i < NUM_CLIENTS; i++)
            threads.emplace_back(read_worker, i, std::ref(counter),
                                 NUM_CLIENTS * MULTI_CLIENT_TXNS, rr);

        for (auto &t : threads)
            t.join();

        auto end = Clock::now();

        double sec = durationSec(start, end);

        std::cout << (rr ? "REPEATABLE_READ" : "READ_COMMITTED")
                  << " TPS: "
                  << (NUM_CLIENTS * MULTI_CLIENT_TXNS) / sec
                  << "\n";
    }
}

// ============================================================
// BENCHMARK 5: RECOVERY TIME
// ============================================================

void benchmark_recovery()
{
    std::cout << "\n=== BENCHMARK 5: Recovery Time ===\n";

    Client client(HOST, PORT);

    // warm DB
    for (int i = 0; i < 100000; i++)
    {
        client.begin();
        client.put("k1", std::to_string(i));
        client.commit();
    }

    std::cout << "Now kill server manually and restart it...\n";
    std::cout << "Press ENTER when ready to measure recovery time\n";
    std::cin.get();

    auto start = Clock::now();

    Client test(HOST, PORT);
    std::string val;
    test.begin();
    test.get("k1", val);
    test.commit();

    auto end = Clock::now();

    std::cout << "Recovery test completed in "
              << durationSec(start, end)
              << " sec (approx)\n";
}

// ============================================================
// MAIN DRIVER
// ============================================================

int main()
{
    std::cout << "\n==============================\n";
    std::cout << " KVDB BENCHMARK SUITE\n";
    std::cout << "==============================\n";

    benchmark_single_client();
    benchmark_no_group_commit();
    benchmark_group_commit();
    benchmark_read_heavy();
    benchmark_recovery();

    std::cout << "\n=== BENCHMARK COMPLETE ===\n";

    return 0;
}