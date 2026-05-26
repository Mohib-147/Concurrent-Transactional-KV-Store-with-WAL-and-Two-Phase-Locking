#ifndef GROUP_COMMIT_MANAGER_H
#define GROUP_COMMIT_MANAGER_H

#include "wal_manager.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>

class GroupCommitManager
{
public:
    GroupCommitManager(WALManager &wal, int interval_ms = 5);
    ~GroupCommitManager();

    void start();
    void stop();

    void waitForCommit(uint64_t commit_lsn);

private:
    WALManager &wal_;

    std::thread worker_;
    std::atomic<bool> running_{false};

    int interval_ms_;

    std::mutex mtx_;
    std::condition_variable cv_;

    void run();

    void flushOnce();

    uint64_t parseLSN(const std::string &rec)
    {
        if (rec.size() < 12)
            throw std::runtime_error("WAL record too short for LSN");
        uint64_t lsn = 0;
        for (int i = 0; i < 8; ++i)
            lsn |= (uint64_t(uint8_t(rec[4 + i])) << (8 * i));
        return lsn;
    }
};

#endif