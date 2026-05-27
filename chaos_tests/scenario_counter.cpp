#include "client.h"
#include <thread>
#include <vector>
#include <iostream>
#include <atomic>

static const std::string KEY = "counter";

// ---------------- worker ----------------

void worker(int id, int ops)
{
    Client client("127.0.0.1", 7000);

    if (!client.isConnected())
    {
        std::cout << "[client " << id << "] connection failed\n";
        return;
    }

    for (int i = 0; i < ops; i++)
    {
        while (true)
        {
            // BEGIN
            std::string beginResp = client.begin();
            if (beginResp.find("ERROR") != std::string::npos)
                continue;

            // GET
            std::string val_str;
            std::string getResp = client.get(KEY, val_str);
            if (getResp.find("ERROR") != std::string::npos)
            {
                client.abort();
                continue;
            }

            int val = 0;
            if (!val_str.empty())
                val = std::stoi(val_str);

            val++;

            // PUT
            std::string putResp = client.put(KEY, std::to_string(val));
            if (putResp.find("ERROR") != std::string::npos)
            {
                client.abort();
                continue;
            }

            // COMMIT
            std::string commitResp = client.commit();
            if (commitResp.find("ERROR") != std::string::npos)
            {
                continue; // retry whole transaction
            }

            break; // success
        }
    }

    client.disconnect();
}

// ---------------- scenario runner ----------------

bool runScenarioCounter()
{
    std::cout << "[scenario 1] concurrent counter test starting...\n";

    const int clients = 8;
    const int ops_per_client = 1000;

    // initialize counter
    Client init("127.0.0.1", 7000);

    init.begin();
    init.put(KEY, "0");
    init.commit();
    init.disconnect();

    // workers
    std::vector<std::thread> threads;

    for (int i = 0; i < clients; i++)
    {
        threads.emplace_back(worker, i, ops_per_client);
    }

    for (auto &t : threads)
        t.join();

    // verification
    Client verifier("127.0.0.1", 7000);

    std::string final_val;
    verifier.begin();
    verifier.get(KEY, final_val);
    verifier.commit();

    verifier.disconnect();

    int result = 0;
    if (!final_val.empty())
        result = std::stoi(final_val);

    std::cout << "[scenario 1] final counter = " << result << "\n";

    int expected = clients * ops_per_client;

    if (result == expected)
    {
        std::cout << "[scenario 1] PASSED\n";
        return true;
    }
    else
    {
        std::cout << "[scenario 1] FAILED (expected " << expected << ")\n";
        return false;
    }
}