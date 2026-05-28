cmake_minimum_required(VERSION 3.16)
project(kvdb CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O0")

# Compile commands (IDE support)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

find_package(Threads REQUIRED)

# ============================================================================
# CORE DATABASE ENGINE (INCLUDES EVERYTHING)
# ============================================================================

file(GLOB CORE_SRC
    src/core/*.cpp
    src/protocol/*.cpp
)

add_library(kvdb_core ${CORE_SRC})

target_include_directories(kvdb_core PUBLIC include src/core src/protocol)
target_link_libraries(kvdb_core PUBLIC Threads::Threads)

# ============================================================================
# SERVER
# ============================================================================

add_executable(kvdb-server
    src/server/main.cpp
    src/server/server.cpp
    src/server/connection_handler.cpp
)

target_include_directories(kvdb-server PRIVATE include)
target_link_libraries(kvdb-server PRIVATE kvdb_core Threads::Threads)

# ============================================================================
# CLIENT (PRODUCTION CLIENT)
# ============================================================================

add_executable(kvdb-cli
    src/client/main.cpp
    src/client/client.cpp
)

target_include_directories(kvdb-cli PRIVATE include)
target_link_libraries(kvdb-cli PRIVATE kvdb_core Threads::Threads)

# ============================================================================
# CHAOS TEST DRIVER
# ============================================================================

file(GLOB CHAOS_SRC
    chaos_tests/*.cpp
)

add_executable(chaos_driver ${CHAOS_SRC})

target_include_directories(chaos_driver PRIVATE include chaos_tests)
target_link_libraries(chaos_driver PRIVATE kvdb_core Threads::Threads)

# ============================================================================
# UNIT TESTS
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