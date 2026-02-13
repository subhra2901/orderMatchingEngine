# DESIGN — Order Matching Engine

This document describes the high-level architecture, data structures, matching logic, concurrency model, and considerations for tuning and scaling the order matching engine.

## Components

- MatchingEngine
  - Core component that receives `Order` objects and produces `Trade` events.
  - Keeps a map of `OrderBook` instances keyed by symbol.

- OrderBook
  - Maintains `buy_orders_` (map<Price, list<Order*>>) and `sell_orders_`.
  - Price maps: buy map sorted ascending (use rbegin for best bid), sell map sorted ascending for best ask.
  - `order_lookup_` provides O(1) access for cancellation.

- ClientGateway
  - Accepts TCP connections via `TcpServer`.
  - Parses incoming binary messages (see `protocol.h`) and dispatches to engine.
  - Serializes execution reports and market data snapshots back to clients.

- TcpServer
  - Multi-threaded socket accept/read loop.
  - Provides callbacks for connection, disconnection, and message arrival.

- Logger
  - Streaming macros: `LOG_INFO << ...`.
  - Configurable minimum log level, flush semantics for tests.

- ObjectPool
  - Pre-allocates `Order` objects to reduce heap churn under high load.

## Data Structures

- Price-level maps: `std::map<Price, std::list<Order*>>` (or `std::map`/`std::multimap`) for deterministic ordering.
- OrderInfo: small struct to hold `(Order* order_ptr, list<Order*>::iterator list_it)` for quick cancellation.
- Trade history: `std::vector<Trade>` for recent trades (consider ring buffer if unbounded growth is a concern).

## Matching Algorithm

- Price-time priority:
  - For a buy LIMIT order, match with lowest asks <= buy.price starting at best ask.
  - For a sell LIMIT order, match with highest bids >= sell.price starting at best bid.
  - Market orders match against best available prices until filled or book depleted.
  - For FOK orders: verify liquidity before matching (scan L2 until required quantity found).
  - For IOC orders: match immediately and cancel any remainder.

- Trade creation:
  - `create_trade(buy, sell, qty, price)` updates stats and appends to history.

## Concurrency & Threading

- TcpServer may use worker threads for I/O; however, matching logic should avoid concurrent conflicting modifications to the same `OrderBook`.
- Strategies:
  1. Fine-grained locking: per-symbol mutex (fast when symbols are partitioned).
  2. Actor model: single-threaded matching per symbol using task queues — avoids locks for book operations.
  3. Global lock: simplest but will limit throughput as concurrency grows.

- Recommended for scale: per-symbol mutex or sharded symbol assignment to matching threads.

## Memory & Performance Considerations

- Use `ObjectPool<Order>` to avoid allocations for every incoming order under high throughput.
- Avoid heavy std::string concatenations on hot paths — prefer streaming logging and pre-sized buffers.
- Minimize lock hold time: collect matching results then update shared state and notify outside the lock if possible.
- Consider batching messages from TcpServer to reduce syscall overhead.

## Observability & Metrics

- Expose stats: `total_orders`, `total_trades`, `total_volume` (already present in engine).
- Consider adding per-symbol counters and histograms for latencies (order handling, matching time).
- Add health endpoints / metric sink (Prometheus) for long-running deployments.

## Fault Handling & Validation

- Validate orders: non-zero quantity, valid symbol, limit price > 0 for limit orders.
- For unexpected errors, log at ERROR and continue to serve other clients.
- For event-replay mode, avoid sending client notifications and skip side-effects as necessary.

## Testing & Benchmarks

- Unit tests (gtest) cover matching logic and cancellation semantics.
- Benchmarks should target:
  - Single-threaded matching latency
  - Multi-threaded client submissions and TCP handling
  - Memory usage patterns under sustained load

## Stress Test Notes & Tuning (based on provided result)

Provided baseline:
```
Threads: 5 | Orders/Thread: 2000 | Total: 10000
Throughput: 14111 ORDERS/SEC
```

Interpretation & tuning tips:
- If throughput is limited by locking, try sharding symbols across worker threads or use per-symbol mutexes.
- If order allocation is a bottleneck, increase object pool size and reuse objects aggressively.
- If syscalls dominate, batch network I/O or use `sendmmsg`/`recvmmsg` (platform dependent).
- Profile CPU hotspots (use `perf`, VTune, or equivalent) to identify expensive code paths.

## Extension Points

- Persistence: add WAL to persist order state for restart/replay.
- Matching policies: add alternative matching engines (pro-rata, FIFO variations).
- Market data: add subscription filtering and deltas instead of full snapshots.

## Safety & Determinism

- Deterministic matching within a single process: ensure consistent ordering of operations and seed any randomness.
- For distributed matching (multiple instances), define partitioning/consensus mechanism to avoid cross-node races.

---

This document is intentionally concise. Ask if you want diagrams (sequence / component diagrams) or a more detailed per-class design sheet.
