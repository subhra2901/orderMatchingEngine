#include <gtest/gtest.h>
#include <matching_engine.h>
class MatchingEngineTest : public ::testing::Test {
protected:
    MatchingEngine engine;
    //Helpers to create orders
    Order makeOrder(OrderID id, Symbol symbol, OrderSide side,Price price, Quantity quantity) {
        return Order{
            .id = id,
            .symbol = symbol,
            .side = side,
            .type = OrderType::LIMIT,
            .price = price,
            .quantity = quantity,
            .timestamp = 0
        };
    }
};

//Test_1 :Validate Input Orders
TEST_F(MatchingEngineTest, ValidateOrder){
    Order valid = makeOrder(1,"AAPL",OrderSide::BUY,150.0,100);
    EXPECT_TRUE(engine.validate_order(valid));

    //Invalid Order: Negative Price
    Order invalid_price = makeOrder(2,"AAPL",OrderSide::SELL,-150.0,100);
    EXPECT_FALSE(engine.validate_order(invalid_price));
}
//Test_2: Get or Create Order Book
TEST_F (MatchingEngineTest, GetOrCreateOrderBook){
    OrderBook& book1 = engine.get_or_create_order_book("AAPL");
    OrderBook& book2 = engine.get_or_create_order_book("AAPL");

    EXPECT_EQ(&book1, &book2); //Should return the same order book
}
//Test_3: Process book statistics
TEST_F(MatchingEngineTest, ProcessOrderStats){
    OrderBook& book = engine.get_or_create_order_book("AAPL");
    Order order1 = makeOrder(1,"AAPL",OrderSide::SELL,150.0,100);
    book.add_order(order1);
    EXPECT_EQ(book.getTotalOrders(),1);
}

TEST_F(MatchingEngineTest, ProcessNewOrder){
    Order sell = makeOrder(1,"AAPL",OrderSide::SELL,150.0,100);
    Order buy = makeOrder(2,"AAPL",OrderSide::BUY,150.0,100);

    auto trades1 = engine.process_new_order(sell);
    EXPECT_EQ(trades1.size(),0); //No trades should be generated from the first order

    auto trades2 = engine.process_new_order(buy);
    EXPECT_EQ(trades2.size(),1); //One trade should be generated from the second order
    EXPECT_EQ(trades2[0].quantity,100); //Trade quantity should be 100
    EXPECT_EQ(trades2[0].price,150.0); //Trade price should be 150.0

    OrderBook* book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getTotalOrders(),0); //Both orders should be fully filled and removed from the book
}

TEST_F(MatchingEngineTest, PartialFill){
    Order sell = makeOrder(1,"AAPL",OrderSide::SELL,150.0,100);
    Order buy = makeOrder(2,"AAPL",OrderSide::BUY,150.0,50);

    auto trades1 = engine.process_new_order(sell);
    EXPECT_EQ(trades1.size(),0); //No trades should be generated from the first order

    auto trades2 = engine.process_new_order(buy);
    EXPECT_EQ(trades2.size(),1); //One trade should be generated from the second order
    EXPECT_EQ(trades2[0].quantity,50); //Trade quantity should be 50
    EXPECT_EQ(trades2[0].price,150.0); //Trade price should be 150.0

    OrderBook* book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getTotalOrders(),1); //Sell order should have 50 remaining
    EXPECT_EQ(book->getBestAsk()->remaining_qty(),50); //Best ask should have 50 remaining
}

TEST_F(MatchingEngineTest, PriceImprovement){
    // Sell @ 150
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::SELL, 150.0, 100));

    // Buy @ 155 (Aggressive buy)
    // Should execute at 150 (Best Ask), not 155
    auto trades = engine.process_new_order(makeOrder(2, "AAPL", OrderSide::BUY, 155.0, 100));

    EXPECT_EQ(trades[0].price, 150.0);
}

TEST_F(MatchingEngineTest, FIFOMatching){
    // Sell 100 @ 150
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::SELL, 150.0, 100));
    // Sell 100 @ 150
    engine.process_new_order(makeOrder(2, "AAPL", OrderSide::SELL, 150.0, 100));

    // Buy 150 @ 150
    auto trades = engine.process_new_order(makeOrder(3, "AAPL", OrderSide::BUY, 150.0, 150));

    // First trade should fill order ID 1 completely
    EXPECT_EQ(trades[0].buy_order_id, 3);
    EXPECT_EQ(trades[0].sell_order_id, 1);
    EXPECT_EQ(trades[0].quantity, 100);

    // Second trade should fill order ID 2 partially
    EXPECT_EQ(trades[1].buy_order_id, 3);
    EXPECT_EQ(trades[1].sell_order_id, 2);
    EXPECT_EQ(trades[1].quantity, 50);

    OrderBook* book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getBestAsk()->id, 2); // One sell order should remain
    EXPECT_EQ(book->getTotalOrders(), 1); // One sell order should remain
    EXPECT_EQ(book->getBestAsk()->remaining_qty(), 50); // Remaining quantity should be 50
}

TEST_F(MatchingEngineTest, OrderCancellation){
    // Sell 100 @ 150
    engine.process_new_order(makeOrder(1, "AAPL", OrderSide::SELL, 150.0, 100));

    OrderBook* book = engine.get_order_book("AAPL");
    EXPECT_EQ(book->getTotalOrders(), 1); // One order should be in the book

    // Cancel the order
    bool cancelled = book->cancel_order(1);
    EXPECT_TRUE(cancelled); // The order should be successfully cancelled
    EXPECT_EQ(book->getTotalOrders(), 0); // No orders should remain in the book
    EXPECT_EQ(book->getBestAsk(), nullptr); // No best ask should be available
}
