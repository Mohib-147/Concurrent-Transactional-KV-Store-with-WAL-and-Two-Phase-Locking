#include "client.h"

#include <thread>
#include <iostream>
#include <string>
#include <atomic>
#include <chrono>

static const std::string KEY = "db_project";

static std::string read1_rc, read2_rc;
static std::string read1_rr, read2_rr;

// ---------------- writer ----------------

void writerTask()
{
    Client client("127.0.0.1", 7000);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    client.begin();

    client.put(KEY, "100");

    client.commit();
}

// ---------------- reader (READ COMMITTED) ----------------

void readerRC()
{
    Client client("127.0.0.1", 7000);

    client.begin("READ_COMMITTED");

    client.get(KEY, read1_rc);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    client.get(KEY, read2_rc);

    client.commit();
}

// ---------------- reader (REPEATABLE READ) ----------------

void readerRR()
{
    Client client("127.0.0.1", 7000);

    client.begin("REPEATABLE_READ");

    client.get(KEY, read1_rr);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    client.get(KEY, read2_rr);

    client.commit();
}

// ---------------- scenario runner ----------------

bool runScenarioIsolation()
{
    std::cout << "[scenario 4] isolation test starting...\n";

    // init
    Client init("127.0.0.1", 7000);
    init.begin();
    init.put(KEY, "1");
    init.commit();

    // ---------------- READ COMMITTED ----------------

    std::thread w1(writerTask);
    std::thread r1(readerRC);

    w1.join();
    r1.join();

    bool rc_non_repeatable = (read1_rc != read2_rc);

    std::cout << "[READ_COMMITTED] "
              << "r1=" << read1_rc
              << " r2=" << read2_rc << "\n";

    // reset value
    init.begin();
    init.put(KEY, "1");
    init.commit();

    // ---------------- REPEATABLE READ ----------------

    std::thread w2(writerTask);
    std::thread r2(readerRR);

    w2.join();
    r2.join();

    bool rr_repeatable = (read1_rr == read2_rr);

    std::cout << "[REPEATABLE_READ] "
              << "r1=" << read1_rr
              << " r2=" << read2_rr << "\n";

    // ---------------- validation ----------------

    bool ok = true;

    if (!rc_non_repeatable)
    {
        std::cout << "[FAIL] READ_COMMITTED did NOT show anomaly\n";
        ok = false;
    }

    if (!rr_repeatable)
    {
        std::cout << "[FAIL] REPEATABLE_READ is broken (non-repeatable read occurred)\n";
        ok = false;
    }

    if (ok)
        std::cout << "[scenario 4] PASSED\n";
    else
        std::cout << "[scenario 4] FAILED\n";

    return ok;
}