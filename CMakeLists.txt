cmake_minimum_required(VERSION 3.16)
project(kvdb CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags for debugging and warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O0")

# Generate compile_commands.json for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include directories
include_directories(include)

# Find pthread (cross-platform way)
find_package(Threads REQUIRED)

# ============================================================================
# SERVER EXECUTABLE
# ============================================================================
add_executable(kvdb-server
    src/server/main.cpp
    src/server/server.cpp
    src/server/connection_handler.cpp
    src/core/lock_manager.cpp
    src/core/transaction_manager.cpp
    src/core/store.cpp
    src/protocol/parser.cpp
    src/protocol/protocol.cpp
)

target_link_libraries(kvdb-server PRIVATE Threads::Threads)
target_include_directories(kvdb-server PRIVATE include)

# ============================================================================
# CLIENT EXECUTABLE
# ============================================================================
add_executable(kvdb-cli
    src/client/main.cpp
    src/client/client.cpp
)

target_link_libraries(kvdb-cli PRIVATE Threads::Threads)
target_include_directories(kvdb-cli PRIVATE include)

# ============================================================================
# TEST EXECUTABLES
# Each test file has its own main() — build them as separate targets.
# ============================================================================
add_executable(test_lock_manager
    tests/test_lock_manager.cpp
    src/core/lock_manager.cpp
    src/core/store.cpp
)
target_include_directories(test_lock_manager PRIVATE include src/core)
target_link_libraries(test_lock_manager PRIVATE Threads::Threads)

add_executable(test_transaction_manager
    tests/test_transaction_manager.cpp
    src/core/lock_manager.cpp
    src/core/transaction_manager.cpp
    src/core/store.cpp
)
target_include_directories(test_transaction_manager PRIVATE include src/core)
target_link_libraries(test_transaction_manager PRIVATE Threads::Threads)

add_executable(test_concurrent_basic
    tests/test_concurrent_basic.cpp
    src/core/lock_manager.cpp
    src/core/transaction_manager.cpp
    src/core/store.cpp
)
target_include_directories(test_concurrent_basic PRIVATE include src/core)
target_link_libraries(test_concurrent_basic PRIVATE Threads::Threads)