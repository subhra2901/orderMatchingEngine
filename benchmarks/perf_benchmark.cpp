#include<benchmark/benchmark.h>
#include <matching_engine.h>
#include <chrono>
#include <random>

static void BM_ProcessNewOrder(benchmark::State& state) {
        MatchingEngine engine;
        OrderBook& book = engine.get_or_create_order_book("AAPL");
        Order sell_order{
                .id = 1,
                .symbol = "AAPL",
                .side = OrderSide::SELL,
                .type = OrderType::LIMIT,
                .price = 150.0,
                .quantity = 10,
                .timestamp = 0
        };
        Order buy_order{
                .id = 2,
                .symbol = "AAPL",
                .side = OrderSide::BUY,
                .type = OrderType::LIMIT,
                .price = 150.0,
                .quantity = 10,
                .timestamp = 0
        };

        // Benchmark processing new buy orders
        for (auto _ : state) {
                engine.process_new_order(sell_order);
                engine.process_new_order(buy_order);
        }
        state.SetItemsProcessed(state.iterations() * 2); // Each iteration processes 2 orders
}
BENCHMARK(BM_ProcessNewOrder)->Unit(benchmark::kNanosecond);
BENCHMARK_MAIN();
    