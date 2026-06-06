#include "group_commit.h"
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

GroupCommitManager::GroupCommitManager(WALManager &wal, int interval_ms)
    : wal_(wal), interval_ms_(interval_ms)
{
}

GroupCommitManager::~GroupCommitManager()
{
    stop();
}

void GroupCommitManager::start()
{
    running_ = true;
    worker_ = std::thread(&GroupCommitManager::run, this);
}

void GroupCommitManager::stop()
{
    running_ = false;

    if (worker_.joinable())
        worker_.join();
}

void GroupCommitManager::run()
{
    while (running_)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_ms_));

        flushOnce();
    }
}

void GroupCommitManager::flushOnce()
{
    auto records = wal_.fetchAndClearBuffer();

    if (records.empty())
        return;

    wal_.writeToDisk(records);

    wal_.fsync();

    uint64_t max_lsn = 0;
    for (const auto &rec : records)
    {
        uint64_t lsn = parseLSN(rec);
        max_lsn = std::max(max_lsn, lsn);
    }

    wal_.updateFlushedLSN(max_lsn);
    cv_.notify_all();
}

void GroupCommitManager::waitForCommit(uint64_t commit_lsn)
{
    std::unique_lock<std::mutex> lock(mtx_);

    cv_.wait(lock, [&]
             { return wal_.getFlushedLSN() >= commit_lsn; });
}