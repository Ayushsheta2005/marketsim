cmake_minimum_required(VERSION 3.20)
project(marketsim CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Library: MarketSim core
add_library(marketsim_lib
    src/OrderBook.cpp
    src/MarketDataReplay.cpp
)

target_include_directories(marketsim_lib PUBLIC include)

# Test executables
add_executable(test_order_book tests/test_order_book.cpp)
target_link_libraries(test_order_book marketsim_lib)
add_test(NAME OrderBookTests COMMAND test_order_book)

add_executable(test_market_data_replay tests/test_market_data_replay.cpp)
target_link_libraries(test_market_data_replay marketsim_lib)
add_test(NAME MarketDataReplayTests COMMAND test_market_data_replay)

# Benchmark executables
add_executable(bench_order_book benchmarks/bench_order_book.cpp)
target_link_libraries(bench_order_book marketsim_lib)

add_executable(bench_order_book_latency benchmarks/bench_order_book_latency.cpp)
target_link_libraries(bench_order_book_latency marketsim_lib)

add_executable(bench_deep_order_book benchmarks/bench_deep_order_book.cpp)
target_link_libraries(bench_deep_order_book marketsim_lib)

add_executable(bench_cancel_stress benchmarks/bench_cancel_stress.cpp)
target_link_libraries(bench_cancel_stress marketsim_lib)

# Validator executables
add_executable(marketsim_validate src/marketsim_validate.cpp)
target_link_libraries(marketsim_validate marketsim_lib)

add_executable(marketsim_validate_transition src/marketsim_validate_transition.cpp)
target_link_libraries(marketsim_validate_transition marketsim_lib)

# Demo executable
add_executable(marketsim_demo src/marketsim_demo.cpp)
target_link_libraries(marketsim_demo marketsim_lib)

# Set compiler flags
if(MSVC)
    target_compile_options(marketsim_lib PRIVATE /W4)
else()
    target_compile_options(marketsim_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Release build optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(marketsim_lib PRIVATE /O2)
    else()
        target_compile_options(marketsim_lib PRIVATE -O3)
    endif()
endif()
