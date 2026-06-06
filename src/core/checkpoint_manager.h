#ifndef CHECKPOINT_MANAGER_H
#define CHECKPOINT_MANAGER_H

#include "store.h"
#include "wal_manager.h"
#include "server.h"

#include <memory>
#include <string>

class CheckpointManager
{
public:
    CheckpointManager(std::shared_ptr<Store> store,
                      std::shared_ptr<WALManager> wal,
                      std::shared_ptr<Server> server,
                      const std::string &data_file_path,
                      const std::string &wal_file_path);

    bool createCheckpoint();

private:
    std::shared_ptr<Store> store_;
    std::shared_ptr<WALManager> wal_;
    std::shared_ptr<Server> server_;

    std::string data_file_path_;
    std::string wal_file_path_;

    void writeSnapshotFile(const std::string &temp_path,
                           uint64_t checkpoint_lsn);

    void truncateWAL(uint64_t checkpoint_lsn);
};

#endif