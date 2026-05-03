cmake_minimum_required(VERSION 3.16)
project(kvdb CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags for debugging and warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O0")

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
    src/core/transactional_manager.cpp
    src/core/store.cpp
    src/protocol/parser.cpp
    src/protocol/protocol.cpp
)

target_link_libraries(kvdb-server PRIVATE Threads::Threads)

# ============================================================================
# CLIENT EXECUTABLE
# ============================================================================
add_executable(kvdb-cli
    src/client/main.cpp
    src/client/client.cpp
)

target_link_libraries(kvdb-cli PRIVATE Threads::Threads)

# ============================================================================
# TESTS EXECUTABLE
# ============================================================================
add_executable(test_kvdb
    tests/test_lock_manager.cpp
    tests/test_transaction_manager.cpp
    tests/test_concurrent_basic.cpp
    src/core/lock_manager.cpp
    src/core/transactional_manager.cpp
    src/core/store.cpp
)

target_link_libraries(test_kvdb PRIVATE Threads::Threads)