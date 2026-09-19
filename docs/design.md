# MarketSim Architecture

**Author:** Ayush Sheta · [github.com/Ayushsheta2005](https://github.com/Ayushsheta2005)

---

## System Goals

Build the systems layer of an electronic exchange — not a trading strategy or price predictor. Specifically:

1. **Correctness:** Price-time priority matching with full fill / partial fill / cancel semantics
2. **Measurable latency:** Nanosecond-resolution benchmarks that survive methodological audit
3. **Real-data validation:** Replay real NASDAQ order flow and compare against exchange snapshots
4. **Clean separation:** Input parsing is independent of the matching engine; each can be tested alone

---

## Core Data Structures

### Order storage: contiguous slot pool

All orders live in a single `vector<OrderNode>`. Each node stores:

```cpp
struct OrderNode {
    OrderId  id;
    Side     side;
    Price    price;
    Quantity quantity;
    size_t   prev, next;   // intrusive doubly-linked list indices
};
```

Nodes form a FIFO queue per price level — no heap allocation per order, no pointer indirection. A hash map (`order_id → slot_index`) enables O(1) node lookup; the doubly-linked list gives O(1) unlink. A free-list recycles cancelled slots so repeated add/cancel cycles allocate no new memory once the pool is warm.

**Why not `std::list`?**  
An earlier implementation used `std::list` for price-level queues. It was slower than the O(n) vector scan it replaced: heap-allocated nodes scattered across memory destroyed cache locality. The slot pool achieves O(1) unlink while preserving the contiguous layout.

### Price levels: ordered maps

```cpp
std::map<Price, PriceLevel> bids_;  // descending best price
std::map<Price, PriceLevel> asks_;  // ascending best price
```

`std::map` gives O(log P) access to the best price and O(log P) insertion / removal of price levels, where P is the number of distinct active prices. For real market data this is typically small.

---

## Matching Logic

On `add_order()`:

1. Determine aggressor side (incoming) vs. resting side (book)
2. While there are resting orders at a price that crosses the incoming:
   - Pop the front of the resting queue (FIFO priority)
   - Compute fill quantity = min(incoming remaining, resting quantity)
   - Generate a Trade event
   - Reduce or remove the resting order
   - Reduce the incoming order
3. If any incoming quantity remains, insert it as a resting order

All fills are generated before any insertion — an incoming order never crosses itself.

---

## Benchmarking Methodology

### Why batch timing?

This machine's monotonic clock ticks at ~41.67 ns. An `add_order` call costs ~30 ns — shorter than one clock tick. Per-event timing would quantize every sample to 42 ns or 84 ns, making the histogram meaningless.

Solution: time 128-event batches. Each sample covers ~96 clock ticks, giving sub-tick resolution of the per-event mean.

Both methods run side-by-side every invocation so the quantization artifact stays visible alongside the correct measurement.

### Cancellation scaling

`bench_cancel_stress` rests N orders at one price level and cancels them front-to-back (worst case for a position-shifting implementation). Measured across 5,000 to 80,000 orders; the slot-pool implementation stays flat throughout.

---

## Market Data Replay

### CSV replay

`replay_market_data()` reads a simple CSV: each row is `Timestamp, OrderID, Side, Price, Quantity, Type`. Types are add, cancel, and trade. Useful for synthetic workloads.

### LOBSTER replay

LOBSTER message files are 6-column CSVs from reconstructed NASDAQ TotalView-ITCH order flow:

```
Time, Type, OrderID, Size, Price, Direction
```

Types handled:
- **1** — new limit order (add to book)
- **2** — partial cancel (reduce resting quantity in place)
- **3** — deletion (remove order)
- **4** — visible execution (reduce resting order; already matched in source data, not re-matched)
- **5, 6, 7** — hidden execution, cross trade, halt (counted but do not mutate visible book)

LOBSTER timestamps use fixed-point arithmetic to avoid floating-point rounding.

### Validation modes

**Full reconstruction** (`marketsim_validate`): starts from an empty book, applies every message, compares the resulting snapshot at each step.

**Transition validation** (`marketsim_validate_transition`): takes row 1 as an authoritative baseline, then checks whether message row k explains the transition from snapshot k-1 to snapshot k. Required because the regular-session LOBSTER file does not contain all resting orders established before the session — a full reconstruction from empty cannot match the first snapshot.

---

## Namespace

All public types live in `namespace market_sim`:

```cpp
market_sim::OrderBook
market_sim::Order
market_sim::Trade
market_sim::replay_market_data()
market_sim::validate_market_data_transitions()
```

---

## File Map

| File | Responsibility |
|---|---|
| `include/Order.hpp` | Order, Trade, Side, Price, Quantity types |
| `include/OrderBook.hpp` | OrderBook, PriceLevel, OrderNode, ReduceResult |
| `include/MarketDataReplay.hpp` | ReplaySummary, LobsterSummary, validation summaries |
| `src/OrderBook.cpp` | add, cancel, reduce, match, best_bid/ask |
| `src/MarketDataReplay.cpp` | CSV parser, LOBSTER parser, validators |
| `src/marketsim_demo.cpp` | LOBSTER replay demo binary |
| `src/marketsim_validate.cpp` | Full-reconstruction validator binary |
| `src/marketsim_validate_transition.cpp` | Transition validator binary |
| `benchmarks/bench_order_book.cpp` | Aggregate throughput |
| `benchmarks/bench_order_book_latency.cpp` | p50/p95/p99 |
| `benchmarks/bench_deep_order_book.cpp` | Multi-level book workload |
| `benchmarks/bench_cancel_stress.cpp` | Cancel cost vs. depth |
| `tests/test_order_book.cpp` | Matching correctness suite |
| `tests/test_market_data_replay.cpp` | Replay and validation tests |
