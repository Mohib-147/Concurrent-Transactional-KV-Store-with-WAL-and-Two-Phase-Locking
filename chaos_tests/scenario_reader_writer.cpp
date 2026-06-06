#include "client.h"

#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <iostream>
#include <mutex>
#include <unordered_set>
#include <chrono>

static const std::string KEY = "x";

// ---------------- shared tracking ----------------

static std::mutex g_mutex;
static std::unordered_set<int> written_values;
static std::vector<int> observed_reads;

// ---------------- writer ----------------

void writerTask(std::atomic<bool> &running, int id)
{
    Client client("127.0.0.1", 7000);

    std::mt19937 rng(id + 123);
    std::uniform_int_distribution<int> dist(1, 100000);

    while (running.load())
    {
        int value = dist(rng);

        if (client.begin().find("ERROR") != std::string::npos)
            continue;

        if (client.put(KEY, std::to_string(value)).find("ERROR") != std::string::npos)
        {
            client.abort();
            continue;
        }

        std::string commitResp = client.commit();

        if (commitResp.find("ERROR") == std::string::npos)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            written_values.insert(value);
        }
    }
}

// ---------------- reader ----------------

void readerTask(std::atomic<bool> &running, int id)
{
    Client client("127.0.0.1", 7000);

    while (running.load())
    {
        if (client.begin().find("ERROR") != std::string::npos)
            continue;

        std::string val;
        std::string getResp = client.get(KEY, val);

        client.commit();

        if (getResp.find("ERROR") == std::string::npos && !val.empty())
        {
            try
            {
                int v = std::stoi(val);

                std::lock_guard<std::mutex> lock(g_mutex);
                observed_reads.push_back(v);
            }
            catch (...)
            {
            }
        }
    }
}

// ---------------- verification ----------------

bool verifyReaderWriter()
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (written_values.empty())
    {
        std::cout << "[FAIL] no writes observed\n";
        return false;
    }

    for (int v : observed_reads)
    {
        if (written_values.find(v) == written_values.end())
        {
            std::cout << "[FAIL] invalid read detected: " << v << "\n";
            return false;
        }
    }

    std::cout << "[PASS] reader-writer consistency verified\n";
    return true;
}

// ---------------- scenario runner ----------------

bool runScenario3()
{
    std::atomic<bool> running(true);
    std::vector<std::thread> threads;

    // 4 writers
    for (int i = 0; i < 4; i++)
        threads.emplace_back(writerTask, std::ref(running), i);

    // 12 readers
    for (int i = 0; i < 12; i++)
        threads.emplace_back(readerTask, std::ref(running), i);

    std::this_thread::sleep_for(std::chrono::seconds(30));

    running.store(false);

    for (auto &t : threads)
        t.join();

    return verifyReaderWriter();
}