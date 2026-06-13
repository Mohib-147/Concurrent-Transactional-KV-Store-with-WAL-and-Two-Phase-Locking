# CMake generated Testfile for 
# Source directory: /home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking
# Build directory: /home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(LockManager "/home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/build/bin/test_lock_manager")
set_tests_properties(LockManager PROPERTIES  _BACKTRACE_TRIPLES "/home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/CMakeLists.txt;93;add_test;/home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/CMakeLists.txt;0;")
add_test(TransactionManager "/home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/build/bin/test_transaction_manager")
set_tests_properties(TransactionManager PROPERTIES  _BACKTRACE_TRIPLES "/home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/CMakeLists.txt;101;add_test;/home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/CMakeLists.txt;0;")
add_test(ConcurrentBasic "/home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/build/bin/test_concurrent_basic")
set_tests_properties(ConcurrentBasic PROPERTIES  _BACKTRACE_TRIPLES "/home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/CMakeLists.txt;109;add_test;/home/mohib/Concurrent-Transactional-KV-Store-with-WAL-and-Two-Phase-Locking/CMakeLists.txt;0;")
