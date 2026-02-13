# Order Matching Engine - Design Document

## System Architecture

### High-Level Overview

```
┌─────────────────────────────────────────────────────────┐
│                   TCP Server                            │
│           (Multi-threaded connection handler)          │
└────────────────────┬────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────┐
│              Client Gateway                             │
│     (Protocol parsing and message dispatch)            │
└────────────────────┬────────────────────────────────────┘
                     │
                     ├─────────────────────────────┐
                     ↓                             ↓
        ┌──────────────────────┐    ┌──────────────────────┐
        │  Matching Engine      │    │   Order Book         │
        │ (Order matching       │    │ (Order storage &     │
        │  and trade           │    │  price aggregation)  │
        │  execution)          │    │                      │
        └──────────────────────┘    └──────────────────────┘
```

## Component Descriptions

### 1. TCP Server (`tcp_server.h/cpp`)

**Responsibility**: Handle network communication

**Key Features**:
- Listens on specified port for incoming client connections
- Creates a thread per client connection
- Forwards raw byte streams to Client Gateway
- Handles connection lifecycle (accept, read, write, close)

**Design Decisions**:
- One thread per connection for simplicity
- Non-blocking I/O where possible
- Graceful shutdown handling

**Thread Safety**: Connection threads are isolated; centralized dispatch via gateway

### 2. Client Gateway (`client_gateway.h/cpp`)

**Responsibility**: Protocol parsing and request dispatch

**Key Features**:
- Parses binary protocol messages
- Authenticates users
- Dispatches requests to matching engine
- Formats and sends responses back to clients
- Maintains client session state

**Message Flow**:
```
Raw Bytes → Parse Header → Validate → Process → Send Response
```

**Message Types Handled**:
- LOGIN_REQUEST → Authenticate → LOGIN_RESPONSE
- NEW_ORDER → Submit to engine → EXECUTION_REPORT
- MARKET_DATA_REQUEST → Query order book → MARKET_DATA_SNAPSHOT
- CLIENT_DISCONNECT → Cleanup → Close connection

### 3. Matching Engine (`matching_engine.h/cpp`)

**Responsibility**: Core matching logic and order execution

**Key Features**:
- Maintains multiple order books (one per symbol)
- Executes matching algorithm
- Generates trade notifications
- Tracks execution history
- Provides market data queries

**Matching Algorithm**:
```
For incoming order:
  1. Find symbol's order book
  2. While remaining quantity > 0:
     a. Get best counter-side order
     b. Match prices (buy price ≥ sell price)
     c. Execute trade (min of quantities)
     d. Update order statuses
     e. Generate execution reports
  3. If quantity remains:
     - Add to order book (unless FOK/IOC)
  4. Return execution status
```

**Trade Execution**:
- Execution price: Best available counter-side price
- Quantity: Minimum of (incoming order qty, best order qty)
- Timestamp: Synchronized across matched orders

### 4. Order Book (`order_book.h/cpp`)

**Responsibility**: Store and organize orders by price level

**Data Structures**:

```cpp
// Buy orders: descending price order
std::map<Price, std::list<Order*>> buy_orders_;
// Example: 100.50 → [Order1, Order2]
//          100.00 → [Order3]

// Sell orders: ascending price order
std::map<Price, std::list<Order*>> sell_orders_;
// Example: 101.00 → [Order4]
//          101.50 → [Order5, Order6]

// Quick lookup by order ID
std::unordered_map<OrderID, OrderInfo> order_lookup_;
```

**Operations**:

| Operation | Complexity | Implementation |
|-----------|-----------|-----------------|
| Add order | O(log N) | Insert into map + list |
| Get best bid/ask | O(1) | Map begin/rbegin |
| Cancel order | O(1) | Direct lookup |
| Find by ID | O(1) | Hash map lookup |
| Market data query | O(D) | Iterate D price levels |

**Design Decisions**:
- Separate buy/sell order maps for efficient best bid/ask access
- Lists within price levels maintain FIFO ordering
- Hash map enables O(1) order cancellation
- Maps maintain automatic price level sorting

## Protocol Design

### Message Format

All messages follow this header structure:
```
Byte 0-1:   Sequence number (uint16_t)
Byte 2:     Message type (uint8_t, ASCII char)
Byte 3-4:   Message length including header (uint16_t)
Byte 5+:    Payload (variable length)
```

**Endianness**: Little-endian throughout

### Message Definitions

#### Login Request
```cpp
Header + [username(20s) + password(20s)]
Total: 45 bytes
```

#### Login Response
```cpp
Header + [status(B) + message(50s)]
Total: 55 bytes
Status: 0=Failed, 1=Success
```

#### New Order Request
```cpp
Header + [client_order_id(Q) + symbol(10s) + side(B) + type(B) + price(d) + quantity(Q)]
Total: 41 bytes
```

#### Execution Report
```cpp
Header + [client_order_id(Q) + execution_id(Q) + symbol(10s) + 
           side(B) + price(d) + quantity(Q) + filled_qty(Q) + status(B)]
Total: 56 bytes
```

#### Market Data Request
```cpp
Header + [symbol(10s)]
Total: 15 bytes
```

#### Market Data Snapshot
```cpp
Header + [symbol(8s) + num_bids(I) + num_asks(I) + bids[](PQ) + asks[](PQ)]
Variable length - up to 10 levels each
```

## Concurrency Model

### Thread Architecture

```
Main Thread
    ↓
TCP Server (main thread) - Accept connections
    ↓
    ├→ Connection Thread 1 - Read client messages
    ├→ Connection Thread 2 - Read client messages
    ├→ Connection Thread N - Read client messages
    
All threads call Client Gateway → Matching Engine (single-threaded)
```

### Synchronization

- **Matching Engine**: Single-threaded (deterministic ordering)
- **Synchronization**: Mutex/atomics protect shared state
- **Order Book Access**: Protected by matching engine's synchronization
- **TCP Server**: Each connection runs on dedicated thread

### Why Single-Threaded Matching?

1. **Deterministic Ordering**: Critical for derivatives/futures
2. **No Race Conditions**: Orders execute in exact sequence
3. **Simple State Management**: No thread-safe data structures needed for matching
4. **Performance**: Single thread can match thousands of orders/sec

## Memory Management

### Order Lifetime

```
1. Client creates order
   ↓
2. Gateway parses and forwards to engine
   ↓
3. Engine adds to order book
   ↓
4. While order is active: referenced in order book + lookup map
   ↓
5. Order cancel/fill: removed from order book
   ↓
6. Order memory freed when client disconnects or after timeout
```

### Optimization: Object Pooling

Optional object pool (`object_pool.h`) pre-allocates order objects:

```cpp
template<typename T>
class ObjectPool {
    std::vector<T> pool_;
    std::queue<T*> available_;
};
```

**Benefits**:
- Reduces allocation latency
- Predictable memory usage
- Easier to debug memory leaks

## Data Flow Examples

### Order Placement Flow

```
Client
  │ (NEW_ORDER message)
  ↓
TCP Server → Connection Thread
  │
  ↓
Client Gateway
  ├─ Parse message
  ├─ Validate input
  ├─ Create Order object
  │
  ↓
Matching Engine
  ├─ Find order book
  ├─ Match against existing orders
  │  ├─ While qty remains and match possible:
  │  │  ├─ Find best counter order
  │  │  ├─ Create trade
  │  │  ├─ Update orders
  │  │  └─ Generate EXECUTION_REPORT
  │  └─ Add remaining qty to order book
  │
  ↓
Client Gateway → TCP Server → Client
  (EXECUTION_REPORT message)
```

### Market Data Query Flow

```
Client
  │ (MARKET_DATA_REQUEST)
  ↓
TCP Server → Connection Thread
  │
  ↓
Client Gateway
  ├─ Parse message
  │
  ↓
Matching Engine
  ├─ Find order book
  ├─ Get L1/L2 quote
  │
  ↓
Client Gateway → TCP Server → Client
  (MARKET_DATA_SNAPSHOT message)
```

## Performance Considerations

### Optimization Strategies

1. **Fast Path for Market Orders**
   - No need to store if fully matched
   - Direct execution without order book entry

2. **Price Level Aggregation**
   - L2 data computed on-demand
   - No need to store pre-calculated levels

3. **Quick Lookups**
   - Hash map for order ID → Order mapping
   - O(1) cancel operations

4. **Cache Locality**
   - Order objects kept together when possible
   - List nodes within each price level

### Benchmarking

Run performance tests:
```bash
./build/bench_ome
```

Typical metrics:
- Order placement: ~50-100μs
- Order matching: ~10-50μs
- Market data query: ~5-25μs

(Varies with system load and hardware)

## Limitations and Future Work

### Current Limitations

1. **Single Symbol Limitation**: Example only; extend for multi-symbol
2. **No Persistence**: Orders lost on shutdown
3. **No Circuit Breakers**: No kill switches for runaway orders
4. **Limited History**: No audit trail
5. **Fixed Order Book Depth**: L2 limited to top 10 levels

### Future Enhancements

1. **Persistence Layer** (SQLite/RocksDB)
   - Store all orders and trades
   - Replay capability on restart

2. **Advanced Order Types**
   - Stop orders
   - Trailing stops
   - Iceberg orders

3. **Risk Management**
   - Position limits
   - Notional exposure limits
   - Kill switches

4. **Analytics**
   - VWAP calculation
   - Volume-weighted stats
   - Historical data querying

5. **Scalability**
   - Sharding by symbol
   - Request queuing
   - Latency percentile tracking

6. **Serialization Options**
   - Protobuf support
   - FIX protocol support

## Testing Strategy

### Unit Tests (`tests/test_matching_engine.cpp`)

- Order book operations
- Matching algorithm correctness
- Order status transitions
- Edge cases (zero quantity, price validation)

### Integration Tests

- Client-server communication
- Full order lifecycle (login → order → execution)
- Concurrent client handling

### Performance Tests (`benchmarks/perf_benchmark.cpp`)

- Throughput: orders/second
- Latency: microseconds per operation
- Memory usage: bytes per order

### Load Testing

```bash
# Run Python client multiple times
for i in {1..100}; do python client/s_client.py & done
```

## Build and Deployment

### Development Build

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4
```

### Release Build

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

Compiler flags for Release:
- `-O3`: Aggressive optimization
- `-march=native`: Use CPU-specific instructions (AVX, etc.)
- `-flto`: Link-time optimization
- `-DNDEBUG`: Remove all assertions

### Deployment

1. Build in Release mode
2. Run tests
3. Profile with benchmarks
4. Deploy executable
5. Configure system:
   - TCP port
   - Log level
   - Resource limits

## Debugging

### Enable Debug Logging

```bash
./build/ome_main 8080 --log-level DEBUG
```

### Common Issues

1. **Port Already in Use**
   - Change port: `./ome_main 9000`

2. **Connection Refused**
   - Ensure server is running
   - Check firewall settings

3. **Protocol Errors**
   - Verify struct packing (pragma pack(push, 1))
   - Check endianness
   - Validate message lengths

4. **Memory Issues**
   - Enable ASAN: `export ASAN_OPTIONS=debug=1:verbosity=2`
   - Run with Valgrind: `valgrind ./ome_main`

## References

- Protocol Buffers: https://developers.google.com/protocol-buffers
- FIX Protocol: https://www.fixtrading.org/
- Order Matching Algorithms: https://en.wikipedia.org/wiki/Order_matching_engine
