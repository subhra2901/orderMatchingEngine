#include<benchmark/benchmark.h>
#include <matching_engine.h>
#include <chrono>

static MatchingEngine engine;

// Benchmark: Process New Order
static void BM_ProcessNewOrder(benchmark::State& state) {
    // Create a sample order
    for (auto _ : state) {
        Order order{
            .id = benchmark::IterationCount(),
            .symbol = "AAPL",
            .side = OrderSide::SELL,
            .type = OrderType::LIMIT,
            .price = 150.0,
            .quantity = 100,
            .timestamp = std::chrono::system_clock::now().time_since_epoch().count()
        };
        // engine.process_new_order(order);
    }
}

BENCHMARK(BM_ProcessNewOrder)->Unit(benchmark::kMicrosecond);
BENCHMARK_MAIN();