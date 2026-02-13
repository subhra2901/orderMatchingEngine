# Order Matching Engine

A high-performance C++ order matching engine with TCP server and Python client for financial trading applications.

## Overview

The Order Matching Engine is a multi-threaded matching engine designed to handle high-frequency order matching with low latency. It features:

- **TCP Server**: Handles client connections and processes trading requests
- **Matching Engine**: Core logic for matching buy and sell orders
- **Order Book**: Maintains buy/sell orders organized by price levels
- **Market Data**: Provides L1 and L2 market data snapshots
- **Logging**: Comprehensive logging system with adjustable log levels
- **Python Client**: Simple client for testing and interaction

## Project Structure

```
.
├── include/                 # Header files
│   ├── protocol.h          # Wire protocol definitions
│   ├── types.h             # Core data types and enumerations
│   ├── order_book.h        # Order book implementation
│   ├── matching_engine.h   # Matching engine logic
│   ├── tcp_server.h        # TCP server interface
│   ├── client_gateway.h    # Client communication gateway
│   ├── object_pool.h       # Memory pooling utility
│   └── order.h             # Order structures
├── src/                     # Source files
│   ├── main.cpp            # Server entry point
│   ├── order_book.cpp      # Order book implementation
│   ├── matching_engine.cpp # Matching engine implementation
│   ├── tcp_server.cpp      # TCP server implementation
│   └── client_gateway.cpp  # Client gateway implementation
├── logging/                 # Logging module
│   ├── logger.hpp          # Logger header
│   └── logger.cpp          # Logger implementation
├── tests/                   # Unit tests
│   └── test_matching_engine.cpp
├── benchmarks/              # Performance benchmarks
│   └── perf_benchmark.cpp
├── client/                  # Python client
│   └── s_client.py         # Simple TCP client for testing
├── CMakeLists.txt          # CMake build configuration
└── README.md               # This file
```

## Building

### Prerequisites

- C++20 compiler (GCC/Clang)
- CMake 3.20+
- Google Test (for tests)
- Google Benchmark (for benchmarks)

### Build Instructions

```bash
# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make

# Build all targets
make all
```

### Build Targets

- `ome_main`: Main server executable
- `test_ome`: Unit tests
- `bench_ome`: Performance benchmarks

## Running the Server

Start the server on default port 8080:

```bash
./build/ome_main
```

Start on a custom port:

```bash
./build/ome_main 9000
```

With custom log level:

```bash
./build/ome_main 8080 --log-level DEBUG
```

## Running Tests

```bash
cd build
ctest
```

Or run the test executable directly:

```bash
./build/test_ome
```

## Running Benchmarks

```bash
./build/bench_ome
```

## Protocol

The server communicates using a binary protocol. See [protocol.h](include/protocol.h) for detailed message structures.

### Message Types

| Type | Code | Direction | Purpose |
|------|------|-----------|---------|
| LOGIN_REQUEST | 'L' | Client → Server | User authentication |
| LOGIN_RESPONSE | 'R' | Server → Client | Authentication response |
| NEW_ORDER | 'N' | Client → Server | Submit new order |
| EXECUTION_REPORT | 'E' | Server → Client | Order execution notification |
| MARKET_DATA_REQUEST | 'M' | Client → Server | Request market data |
| MARKET_DATA_SNAPSHOT | 'S' | Server → Client | Market data response |
| CLIENT_DISCONNECT | 'X' | Client → Server | Disconnect notification |

## Python Client

A simple Python client is provided for testing and interaction.

### Usage

```bash
python client/s_client.py
```

The client:
1. Connects to the server on localhost:8080
2. Sends a login request
3. Places a new order for AAPL
4. Requests market data
5. Displays execution reports and market data updates

## Data Types

### Order Sides
- `BUY` (0): Buy order
- `SELL` (1): Sell order

### Order Types
- `LIMIT` (0): Execute at specific price or better
- `MARKET` (1): Execute immediately at best available price
- `FOK` (2): Fill or Kill - all or nothing
- `IOC` (3): Immediate or Cancel
- `GFD` (4): Good for Day

### Order Status
- `NEW` (0): Order is new
- `PARTIAL` (1): Order has been partially executed
- `FILLED` (2): Order has been completely executed
- `CANCELLED` (3): Order has been cancelled

## Features

### High Performance
- Single-threaded matching logic for deterministic ordering
- Memory pooling to reduce allocations
- Optimized data structures (maps, lists)
- Compile-time optimizations (-O3, LTO, architecture-specific flags)

### Robustness
- Comprehensive logging system
- Error handling and validation
- L1 and L2 market data support
- Multiple order types (Market, Limit, FOK, IOC, GFD)

### Scalability
- TCP multi-threaded server
- Efficient memory management
- Thread-safe operations

## Configuration

### CMakeLists.txt Settings

- `CMAKE_BUILD_TYPE`: Release (optimized)
- `CMAKE_CXX_STANDARD`: 20
- `CMAKE_CXX_FLAGS_RELEASE`: `-O3 -march=native -DNDEBUG -flto`

### Logging Levels

Available log levels (from least to most verbose):
- `ERROR`: Error messages only
- `WARN`: Warnings and errors
- `INFO`: General information
- `DEBUG`: Detailed debug information

## Technical Details

### Matching Algorithm

The matching engine uses a price-time priority matching algorithm:
1. Orders are organized in a map by price level
2. Within each price level, orders are maintained in FIFO order
3. Buy orders are sorted in descending price order
4. Sell orders are sorted in ascending price order

### Market Data

- **L1Quote**: Best bid/ask prices and quantities
- **L2Quote**: Top N price levels (default 10 levels) with bid/ask quantities

## Performance

Expected performance metrics (on modern hardware):

- Order placement: < 100 microseconds
- Order matching: < 50 microseconds  
- Market data updates: < 25 microseconds

(Actual performance depends on hardware and load)

## Future Enhancements

- [ ] Additional order types (stop, trail)
- [ ] Advanced matching algorithms
- [ ] Multi-symbol support optimization
- [ ] Persistence layer
- [ ] Web API interface
- [ ] Real-time metrics/analytics

## License

Proprietary - All rights reserved

## Author

Order Matching Engine Development Team
