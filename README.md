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
# Order Matching Engine

High-performance C++ order matching engine with a TCP gateway and a small Python client for testing.

## Overview

- Core matching engine implementing price-time priority matching
- Per-symbol order books with L1/L2 market data snapshots
- TCP client gateway for binary protocol
- Memory pooling for reduced allocations and higher throughput
- Streaming logging macros (example: `LOG_INFO << "msg" << value;`)

## Project Layout

```
. 
├── include/        # Headers (protocol, types, engine API)
├── src/            # Implementation (engine, books, gateway, server)
├── logging/        # Logger implementation
├── tests/          # Unit tests
├── benchmarks/     # Performance tests
├── client/         # Python test client
├── CMakeLists.txt  # Build
└── README.md       # This file
```

## Build

Requirements: C++20 toolchain, CMake 3.20+, clang-format (optional), gtest for tests.

```bash
mkdir -p build && cd build
cmake ..
cmake --build . --config Release
# Binaries: build/ome_main, build/test_ome, build/bench_ome
```

## Run

Start server (default port 8080):

```bash
./build/ome_main
```

See `src/main.cpp` for available CLI flags (replay, log level, etc.).

## Formatting

- Recommended formatter: `clang-format` with a project `.clang-format` and `.editorconfig`.
- VSCode: set `editor.tabSize` to `4` and `editor.insertSpaces` = `true`.

Example command to format all source files with `clang-format` (PowerShell):

```powershell
Get-ChildItem -Path 'src','include','tests','logging','benchmarks' -Include '*.cpp','*.h' -Recurse |
	ForEach-Object { clang-format -i -style=file $_.FullName }
```

## Logging

- Logging uses streaming macros to avoid intermediate string allocations and to improve readability.
- Example: `LOG_WARN << "Client " << fd << " not logged in";`.

## Stress Test Result (baseline)

Provided stress test (used as baseline for tuning):

```
=== LAUNCHING STRESS TEST ===
Target: 127.0.0.1:8080
Threads: 5 | Orders/Thread: 2000
Total Orders: 10000
--------------------------------
[Client 1] Finished. 2000 orders in 0.59s (3413 orders/sec)
[Client 3] Finished. 2000 orders in 0.60s (3353 orders/sec)
[Client 4] Finished. 2000 orders in 0.60s (3318 orders/sec)
[Client 5] Finished. 2000 orders in 0.60s (3315 orders/sec)
[Client 2] Finished. 2000 orders in 0.61s (3303 orders/sec)
--------------------------------
=== TEST COMPLETE ===
Total Time: 0.7087 seconds
Throughput: 14111 ORDERS PER SECOND.
```

Use this result to compare changes in lock strategy, thread counts, batching, and allocator sizing.

## Tests & Benchmarks

- Unit tests: `build/test_ome`
- Benchmarks: `build/bench_ome`

For design details see DESIGN.md
