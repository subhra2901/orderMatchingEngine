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
    // book.add_order(order1);
    // ExPECT_EQ(book.getTotalOrders(),1);
}