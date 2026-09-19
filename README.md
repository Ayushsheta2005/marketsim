# MarketSim

**A C++20 limit order book and matching engine with nanosecond-level latency benchmarking and real NASDAQ market data replay.**

**Author:** Ayush Sheta · [github.com/Ayushsheta2005](https://github.com/Ayushsheta2005)

---

## What It Does

MarketSim is a trading-infrastructure simulator focused on the systems layer of an electronic exchange — not price prediction or strategy. It ingests order events, maintains a live bid/ask order book, matches orders by price-time priority, generates trade confirmations, and measures end-to-end latency at nanosecond resolution.

Key results on Apple Silicon arm64 · macOS 26.5.1 (CMake Release build, AppleClang 21):

| Metric | Value |
|---|---:|
| Throughput | ~17.0 M events/s |
| Average cost | 58.7 ns/event |
| p50 latency (batched) | 51.1 ns/event |
| p95 latency (batched) | 54.0 ns/event |
| p99 latency (batched) | 97.7 ns/event |
| Cancel latency, 5k–80k orders | flat (28–31 ns) |

Full results: [`results/benchmark_results.md`](results/benchmark_results.md)

---

## Architecture

```
                    ┌─────────────────────────────────┐
  Market data ──►   │   MarketDataReplay               │
  (CSV / LOBSTER)   │   replay_market_data()           │
                    │   replay_market_data()           │
                    └────────────┬────────────────────┘
                                 │ add_order / cancel_order / reduce
                                 ▼
                    ┌─────────────────────────────────┐
                    │   OrderBook                      │
                    │                                  │
                    │  bids: map<Price, PriceLevel>    │
                    │  asks: map<Price, PriceLevel>    │
                    │  pool: vector<OrderNode>  ◄──── slot pool (contiguous)
                    │  order_location: hash_map        │
                    └────────────┬────────────────────┘
                                 │ on crossing order
                                 ▼
                           Trade events
```

**Order storage design:**  
All orders live in a single contiguous `vector<OrderNode>`. Each node carries `prev`/`next` indices, forming an intrusive doubly-linked FIFO per price level — no heap allocation per order, no pointer chasing. Cancellation is O(log P) map lookup + O(1) unlink.

---

## Key Design Decisions

**Why a slot pool instead of `std::list`?**  
An earlier version used `std::list` for price-level queues. It was _slower_ than the O(n) vector scan it replaced: per-node heap allocation scattered orders across memory, destroying cache locality. The slot pool keeps O(1) unlink while preserving contiguous layout.

**Why batch-timing benchmarks?**  
This machine's monotonic clock ticks at ~41.67 ns — longer than the operation being measured. Per-event samples would all land on one or two ticks, making the histogram useless. Timing 128-event batches gives ~96 ticks per sample, enough resolution to see real distribution.

**Why validate against LOBSTER snapshots?**  
A matching engine can look correct on synthetic data and silently mishandle real exchange semantics (type-2 partial cancels, hidden executions, cross trades). Replaying against paired LOBSTER message+orderbook files gives ground truth: 295,828 transitions matched exactly, zero observable mismatches on the AAPL Level-10 sample.

---

## Build

### Prerequisites

- CMake ≥ 3.20
- C++20 compiler (Clang 14+ or GCC 12+)

### Compile

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run tests

```bash
ctest --test-dir build --output-on-failure --no-tests=error
```

### Run everything (configure + build + test + benchmark)

```bash
./scripts/run_all.sh
```

---

## Benchmarks

```bash
./build/bench_order_book 1000000           # aggregate throughput
./build/bench_order_book_latency 100000    # p50 / p95 / p99
./build/bench_deep_order_book 1000000      # multi-level book
./build/bench_cancel_stress                # cancel cost vs. depth
```

## Market Data Replay

```bash
# Demo: replay synthetic LOBSTER-format data
./build/marketsim_demo data/sample_market_message.csv

# Full validation: replay + compare against orderbook snapshots
./build/marketsim_validate data/sample_market_message.csv data/sample_market_orderbook.csv 10

# Transition-based validation (no full reconstruction needed)
./build/marketsim_validate_transition data/sample_market_message.csv data/sample_market_orderbook.csv 10
```

---

## Test Coverage

Tests use an always-active `CHECK` mechanism (enabled in both Debug and Release):

- Non-crossing adds, partial fills, full fills with remainder, both aggressor sides
- Cancel of existing, missing, and already-filled orders
- Volume conservation and non-negative resting quantities
- Linked-list edge cases: cancel head, tail, middle node (FIFO order preserved), sole order at a level
- Slot reuse under heavy add/cancel churn
- CSV and LOBSTER replay parsing via in-memory streams
- Paired LOBSTER full-replay and local transition-validation

---

## Project Layout

```
marketsim/
├── include/
│   ├── Order.hpp              ← Order, Trade, Price types (namespace market_sim)
│   ├── OrderBook.hpp          ← OrderBook, PriceLevel, slot pool
│   └── MarketDataReplay.hpp   ← Replay and validation interfaces
├── src/
│   ├── OrderBook.cpp          ← Matching engine core
│   ├── MarketDataReplay.cpp   ← CSV + LOBSTER parser, validators
│   ├── marketsim_demo.cpp     ← Market data replay demo
│   ├── marketsim_validate.cpp ← Full-reconstruction validator
│   └── marketsim_validate_transition.cpp  ← Transition validator
├── benchmarks/
│   ├── bench_order_book.cpp          ← Aggregate throughput
│   ├── bench_order_book_latency.cpp  ← Percentile latency
│   ├── bench_deep_order_book.cpp     ← Multi-level workload
│   └── bench_cancel_stress.cpp       ← Cancel scaling
├── tests/
│   ├── test_order_book.cpp
│   └── test_market_data_replay.cpp
├── data/
│   ├── sample_market_message.csv     ← Synthetic LOBSTER-format messages
│   └── sample_market_orderbook.csv   ← Paired order-book snapshots
├── results/                          ← Benchmark and validation output
├── docs/
│   └── design.md                     ← Architecture notes
├── scripts/
│   └── run_all.sh                    ← One-shot build + test + benchmark
├── CMakeLists.txt
└── LICENSE                           ← MIT, Ayush Sheta 2026
```

---

## Performance Notes

All numbers are from a CMake Release build on Apple M4 MacBook Pro. Each benchmark discards one warm-up invocation then runs five measured trials.

Cancellation scaling is flat across a 16× depth range (5k → 80k resting orders), consistent with the slot pool's O(1) unlink:

| Resting Orders | Cancel ns/op |
|---:|---:|
| 5,000 | 30.4 |
| 10,000 | 27.5 |
| 20,000 | 29.8 |
| 40,000 | 28.5 |
| 80,000 | 31.4 |

The complete cancel path also does a hash-map lookup and a price-map lookup — not mathematically O(1) end-to-end, but dominated by the O(1) unlink in practice.

Percentile numbers are means over 128-event batches, not individual event latencies.

See [`results/`](results/) for raw trial data, environment details, and methodology.

---

## License

MIT © 2026 Ayush Sheta — [github.com/Ayushsheta2005](https://github.com/Ayushsheta2005)
