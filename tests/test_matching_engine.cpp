#include "logger.hpp"
#include <gtest/gtest.h>
#include <matching_engine.h>

class MatchingEngineTest : public ::testing::Test {
  protected:
    MatchingEngine engine;
    void SetUp() override {
        Logger::getInstance().setMinLevel(LogLevel::DEBUG); // Enable debug logging for tests

        LOG_INFO << "--- Starting Matching Engine Test ---"
                 << ::testing::UnitTest::GetInstance()->current_test_suite()->name() << "."
                 << ::testing::UnitTest::GetInstance()->current_test_info()->name();
    }
    void TearDown() override {
        LOG_INFO << "--- Finished Matching Engine Test ---"
                 << ::testing::UnitTest::GetInstance()->current_test_suite()->name() << "."
                 << ::testing::UnitTest::GetInstance()->current_test_info()->name();
        Logger::getInstance().flush(); // Ensure all logs are flushed after each test
    }
    // Helpers to create orders
    Order makeOrder(OrderID id, Symbol symbol, OrderSide side, OrderType type, Price price,
                    Quantity quantity) {
        return Order{.id        = id,
                     .symbol    = symbol,
                     .side      = side,
                     .type      = type,
                     .price     = price,
                     .quantity  = quantity,
                     .timestamp = 0};
    }
};

// Test_1 :Validate Input Orders
TEST_F(MatchingEngineTest, ValidateOrder) {
    Order valid = makeOrder(1, "AAPL", OrderSide::BUY, OrderType::LIMIT, 150.0, 100);
    EXPECT_TRUE(engine.validate_order(valid));

    // Invalid Order: Negative Price
    Order invalid_price = makeOrder(2, "AAPL", OrderSide::SELL, OrderType::LIMIT, -150.0, 100);
    EXPECT_FALSE(engine.validate_order(invalid_price));
}
// Test_2: Get or Create Order Book
TEST_F(MatchingEngineTest, GetOrCreateOrderBook) {
    OrderBook &book1 = engine.get_or_create_order_book("AAPL");
    OrderBook &book2 = engine.get_or_create_order_book("AAPL");

    EXPECT_EQ(&book1, &book2); // Should return the same order book
}
// Test_3: Process book statistics
TEST_F(MatchingEngineTest, ProcessOrderStats) {
    OrderBook &book = engine.get_or_create_order_book("AAPL");
    Order order1    = makeOrder(1, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 100);
    book.add_order(&order1);
    EXPECT_EQ(book.getTotalOrders(), 1);
}

TEST_F(MatchingEngineTest, ProcessNewOrder) {
    Order sell = makeOrder(1, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 100);
    Order buy  = makeOrder(2, "AAPL", OrderSide::BUY, OrderType::LIMIT, 150.0, 100);

    auto trades1 = engine.process_new_order(sell);
    EXPECT_EQ(trades1.size(),
              0); // No trades should be generated from the first order

    auto trades2 = engine.process_new_order(buy);
    EXPECT_EQ(trades2.size(),
              1);                        // One trade should be generated from the second order
    EXPECT_EQ(trades2[0].quantity, 100); // Trade quantity should be 100
    EXPECT_EQ(trades2[0].price, 150.0);  // Trade price should be 150.0

    OrderBook *book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getTotalOrders(),
              0); // Both orders should be fully filled and removed from the book
}

TEST_F(MatchingEngineTest, PartialFill) {
    Order sell = makeOrder(1, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 100);
    Order buy  = makeOrder(2, "AAPL", OrderSide::BUY, OrderType::LIMIT, 150.0, 50);

    auto trades1 = engine.process_new_order(sell);
    EXPECT_EQ(trades1.size(),
              0); // No trades should be generated from the first order

    auto trades2 = engine.process_new_order(buy);
    EXPECT_EQ(trades2.size(),
              1);                       // One trade should be generated from the second order
    EXPECT_EQ(trades2[0].quantity, 50); // Trade quantity should be 50
    EXPECT_EQ(trades2[0].price, 150.0); // Trade price should be 150.0

    OrderBook *book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getTotalOrders(), 1); // Sell order should have 50 remaining
    EXPECT_EQ(book->getBestAsk()->remaining_qty(),
              50); // Best ask should have 50 remaining
}

TEST_F(MatchingEngineTest, PriceImprovement) {
    // Sell @ 150
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 100));

    // Buy @ 155 (Aggressive buy)
    // Should execute at 150 (Best Ask), not 155
    auto trades = engine.process_new_order(
        makeOrder(2, "AAPL", OrderSide::BUY, OrderType::LIMIT, 155.0, 100));

    EXPECT_EQ(trades[0].price, 150.0);
}

TEST_F(MatchingEngineTest, FIFOMatching) {
    // Sell 100 @ 150
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 100));
    // Sell 100 @ 150
    engine.process_new_order(makeOrder(2, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 100));

    // Buy 150 @ 150
    auto trades = engine.process_new_order(
        makeOrder(3, "AAPL", OrderSide::BUY, OrderType::LIMIT, 150.0, 150));

    // First trade should fill order ID 1 completely
    EXPECT_EQ(trades[0].buy_order_id, 3);
    EXPECT_EQ(trades[0].sell_order_id, 1);
    EXPECT_EQ(trades[0].quantity, 100);

    // Second trade should fill order ID 2 partially
    EXPECT_EQ(trades[1].buy_order_id, 3);
    EXPECT_EQ(trades[1].sell_order_id, 2);
    EXPECT_EQ(trades[1].quantity, 50);

    OrderBook *book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getBestAsk()->id, 2); // One sell order should remain
    EXPECT_EQ(book->getTotalOrders(), 1); // One sell order should remain
    EXPECT_EQ(book->getBestAsk()->remaining_qty(),
              50); // Remaining quantity should be 50
}

TEST_F(MatchingEngineTest, OrderCancellation) {
    // Sell 100 @ 150
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 100));

    OrderBook *book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getTotalOrders(), 1); // One order should be in the book

    // Cancel the order
    bool cancelled = book->cancel_order(1);
    EXPECT_TRUE(cancelled);                 // The order should be successfully cancelled
    EXPECT_EQ(book->getTotalOrders(), 0);   // No orders should remain in the book
    EXPECT_EQ(book->getBestAsk(), nullptr); // No best ask should be available
}

// --------IOC Tests-------- //

TEST_F(MatchingEngineTest, IOC_NoLiquidity) {
    auto trades =
        engine.process_new_order(makeOrder(1, "AAPL", OrderSide::BUY, OrderType::IOC, 150.0, 100));
    EXPECT_EQ(trades.size(), 0); // No trades should be executed
}

TEST_F(MatchingEngineTest, IOC_PartialFill) {
    // Sell 50 @ 150
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 50));

    // Buy 100 @ 150 with IOC
    auto trades =
        engine.process_new_order(makeOrder(2, "AAPL", OrderSide::BUY, OrderType::IOC, 150.0, 100));

    EXPECT_EQ(trades.size(), 1);       // One trade should be executed
    EXPECT_EQ(trades[0].quantity, 50); // Trade quantity should be 50

    OrderBook *book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getTotalOrders(), 0); // No orders should remain in the book
}

// --------FOK Tests-------- //
TEST_F(MatchingEngineTest, FOK_NotEnoughLiquidity) {
    // Sell 50 @ 150
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 50));

    // Buy 100 @ 150 with FOK
    auto trades =
        engine.process_new_order(makeOrder(2, "AAPL", OrderSide::BUY, OrderType::FOK, 150.0, 100));

    EXPECT_EQ(trades.size(), 0); // No trades should be executed

    OrderBook *book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getTotalOrders(),
              1); // Original sell order should still be in the book
}

TEST_F(MatchingEngineTest, FOK_FullFill) {
    // Sell 100 @ 150
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 100));
    engine.process_new_order(makeOrder(2, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 101));

    // Buy 201 @ 150 with FOK
    auto trades =
        engine.process_new_order(makeOrder(3, "AAPL", OrderSide::BUY, OrderType::FOK, 150.0, 201));

    EXPECT_EQ(trades.size(), 2);        // Two trades should be executed
    EXPECT_EQ(trades[0].quantity, 100); // First trade quantity should be 100
    EXPECT_EQ(trades[1].quantity, 101); // Second trade quantity should be 101

    OrderBook *book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getTotalOrders(), 0); // No orders should remain in the book
}

// --------Market Order Tests-------- //
TEST_F(MatchingEngineTest, MarketOrderExecution) {
    // Sell 100 @ 150 and 200 @ 151
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 100));
    engine.process_new_order(makeOrder(2, "AAPL", OrderSide::SELL, OrderType::LIMIT, 151.0, 200));

    // Buy 150 @ Market price (should match at 150)
    auto trades =
        engine.process_new_order(makeOrder(3, "AAPL", OrderSide::BUY, OrderType::MARKET, 0.0, 150));

    EXPECT_EQ(trades.size(), 2);        // Two trades should be executed
    EXPECT_EQ(trades[0].quantity, 100); // First trade quantity should be 100
    EXPECT_EQ(trades[0].price, 150.0);  // First trade price should be 150.0
    EXPECT_EQ(trades[1].quantity, 50);  // Second trade quantity should be 50
    EXPECT_EQ(trades[1].price, 151.0);  // Second trade price should be 151.0

    OrderBook *book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getTotalOrders(),
              1); // One order should remain in the book (the 200 @ 151 sell order)
    EXPECT_EQ(book->getBestAsk()->remaining_qty(),
              150); // Remaining quantity should be 150
}

TEST_F(MatchingEngineTest, MarketOrderNoLiquidity) {
    // Buy 100 @ Market price (should not execute)
    auto trades =
        engine.process_new_order(makeOrder(1, "AAPL", OrderSide::BUY, OrderType::MARKET, 0.0, 100));

    EXPECT_EQ(trades.size(), 0); // No trades should be executed

    OrderBook *book = engine.get_order_book("AAPL");
    if (book) {
        EXPECT_EQ(book->getTotalOrders(), 0); // No orders should be in the book
    } else {
        SUCCEED(); // If no order book exists, that's also correct
    }
}

//--------Edge Case Tests-------- //
TEST_F(MatchingEngineTest, selfMatching) {
    // Buy 100 @ 150
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::BUY, OrderType::LIMIT, 150.0, 100));

    // Sell 100 @ 150 from the same user (should not match with itself)
    auto trades = engine.process_new_order(
        makeOrder(2, "AAPL", OrderSide::SELL, OrderType::LIMIT, 150.0, 100));

    EXPECT_EQ(trades.size(), 1);           // One trade should be executed
    EXPECT_EQ(trades[0].buy_order_id, 1);  // Buy order ID should be 1
    EXPECT_EQ(trades[0].sell_order_id, 2); // Sell order ID should be 2
}
