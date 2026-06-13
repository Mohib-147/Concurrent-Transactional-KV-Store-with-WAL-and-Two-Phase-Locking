# Build and Run Instructions

## Prerequisites
- CMake 3.16 or higher
- C++ 17 compatible compiler (GCC/Clang)
- Linux/Unix environment (uses POSIX sockets)

## Building the Project

### Step 1: Configure and Build
```bash
cd /home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking
mkdir -p build
cd build
cmake ..
make -j4
```

The CMake build will automatically:
- Create `build/wal/` directory for Write-Ahead Logs
- Create `build/data/` directory for snapshots/checkpoints
- Compile all binaries to `build/bin/`

### Step 2: Verify Build
```bash
ls -lah bin/
# Should show: kvdb-server, kvdb-cli, chaos_driver, test_*, libkvdb_core.a
```

---

## Running Tests

### Unit Tests (Automated)

#### Option A: Using CTest (Recommended)
```bash
cd build
ctest --output-on-failure
# Or with verbose output:
ctest --verbose
```

#### Option B: Run Individual Tests Directly
```bash
cd build

# Test 1: Lock Manager
./bin/test_lock_manager
# Expected: "test_lock_manager: all assertions passed"

# Test 2: Transaction Manager
./bin/test_transaction_manager
# Expected: Clean exit (currently a placeholder)

# Test 3: Concurrent Basic
./bin/test_concurrent_basic
# Expected: Clean exit (currently a placeholder)
```

### Chaos Tests (Interactive)

The chaos_driver is an interactive test suite with 8 scenarios:

```bash
cd build
./bin/chaos_driver
```

Menu options:
```
1. Scenario 1 (Counter) - Test basic counter increment
2. Scenario 2 (RMW) - Read-Modify-Write operations
3. Scenario 3 (Reader-Writer) - Concurrent read/write patterns
4. Scenario 4 (Isolation) - Transaction isolation verification
5. Scenario 5 - RUN CRASH TEST - Simulate server crash
6. Scenario 5 - VERIFY AFTER RESTART - Verify recovery
7. Scenario 6 (Deadlock) - Deadlock detection scenarios
8. Run ALL - Execute all tests in sequence
0. Exit - Quit the driver
```

**Note:** Scenario 5 requires manual server restart between runs (see below).

---

## Running the Server and Client

### Terminal 1: Start the Server
```bash
cd /home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/build

# Default port (7000)
./bin/kvdb-server

# Or with custom port
./bin/kvdb-server --port 9000

# Expected output:
# [server] starting up...
# [server] recovery completed successfully
# [server] group commit service started
# [server] listening on port 7000
# [server] ready to accept connections
```

### Terminal 2: Start a Client
```bash
cd /home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/build

./bin/kvdb-cli localhost 7000

# Interactive commands (case-insensitive):
> BEGIN
> PUT key1 value1
> GET key1
> COMMIT
> QUIT
```

### Example Session
```
> BEGIN
> PUT mykey "Hello World"
> PUT counter "42"
> GET mykey
> GET counter
> COMMIT
> BEGIN READ_COMMITTED
> GET mykey
> COMMIT
> QUIT
Disconnecting...
```

---

## Project Structure

```
├── src/
│   ├── core/              # Core database engine
│   │   ├── lock_manager.cpp
│   │   ├── transaction_manager.cpp
│   │   ├── wal_manager.cpp          # Write-Ahead Logging
│   │   ├── store.cpp                # In-memory KV store
│   │   ├── recovery_manager.cpp     # Crash recovery
│   │   ├── checkpoint_manager.cpp   # Periodic snapshots
│   │   └── group_commit.cpp         # Batched flushing
│   ├── server/            # TCP server
│   │   ├── server.cpp
│   │   ├── connection_handler.cpp
│   │   └── main.cpp
│   ├── client/            # Client library
│   │   ├── client.cpp
│   │   └── main.cpp
│   └── protocol/          # Command parsing
│       ├── parser.cpp
│       └── protocol.cpp
├── tests/                 # Unit tests
│   ├── test_lock_manager.cpp
│   ├── test_transaction_manager.cpp
│   └── test_concurrent_basic.cpp
├── chaos_tests/           # Chaos/integration tests
│   ├── main.cpp
│   ├── scenario_*.cpp
│   ├── client.cpp
│   └── reference_model.cpp
├── build/                 # Build artifacts (created by cmake)
│   ├── bin/               # Executables
│   ├── wal/               # Write-ahead logs (runtime)
│   ├── data/              # Data snapshots (runtime)
│   └── CMakeFiles/        # CMake internals
└── CMakeLists.txt         # Build configuration
```

---

## Key Features

### Two-Phase Locking
- SHARED locks for reads
- EXCLUSIVE locks for writes
- Deadlock detection with automatic abort

### Write-Ahead Logging (WAL)
- All changes logged before application
- Location: `build/wal/wal.log`

### Group Commit
- Batches multiple transactions' writes to disk
- Improves performance under high load

### Recovery
- Automatic recovery on startup
- Replays committed transactions from WAL
- Skips aborted transactions

### Isolation Levels
- READ_COMMITTED (default)
- REPEATABLE_READ

---

## Troubleshooting

### Build fails
```bash
# Clean and rebuild
rm -rf build
mkdir build && cd build
cmake .. && make -j4
```

### Server won't start - Port already in use
```bash
# Use different port
./bin/kvdb-server --port 9000
```

### Server won't start - WAL/data directories missing
```bash
# CMake should create these automatically, but you can create manually:
mkdir -p build/wal build/data
```

### Tests show segmentation fault
```bash
# Rebuild with debug symbols
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make clean && make -j4
gdb ./bin/test_lock_manager
```

---

## CMakeLists.txt Updates

The CMakeLists.txt has been updated with:

✅ **Explicit output directories** - Binaries go to `build/bin/`
✅ **Runtime directories** - Auto-creates `wal/` and `data/` directories
✅ **CTest integration** - Run tests with `ctest` command
✅ **Better organization** - Clear sections for each component

### Key changes:
```cmake
# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Create runtime directories
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/wal")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/data")

# Enable testing
enable_testing()
add_test(NAME LockManager COMMAND test_lock_manager)
# ... etc
```
