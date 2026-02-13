#pragma once
#include <cstdint>

#pragma pack(push, 1)

// Message Type
enum class MessageType : uint8_t {
    LOGIN_REQUEST        = 'L',
    LOGIN_RESPONSE       = 'R',
    NEW_ORDER            = 'N',
    EXECUTION_REPORT     = 'E',
    ORDER_CANCEL         = 'C',
    MARKET_DATA_REQUEST  = 'M',
    MARKET_DATA_SNAPSHOT = 'S',
    SUBSCRIPTION_REQUEST = 'Q',
    TRADE_UPDATE         = 'T',
    CLIENT_DISCONNECT    = 'X'
};

// Header for all messages
struct MessageHeader {
    uint16_t seq_num;
    MessageType type;
    uint16_t msg_len;
};

// CLIENT -> SERVER: Login Request
struct LoginRequest {
    MessageHeader header;
    char username[20];
    char password[20];
};

struct NewOrderRequest {
    MessageHeader header;
    uint64_t client_order_id;
    char symbol[10];
    uint8_t side; // 0=Buy, 1=Sell
    uint8_t type; // 0=Market, 1=Limit
    double price; // Only for Limit orders
    uint64_t quantity;
};

// SERVER -> CLIENT: Login Response
struct LoginResponse {
    MessageHeader header;
    uint8_t status;   // 0=Fail, 1=Success
    char message[50]; // Optional message
};

struct MarketDataRequest {
    MessageHeader header;
    char symbol[10];
};

struct L2Entry {
    double price;
    uint64_t quantity;
};

struct MarketDataSnapshot {
    MessageHeader header;
    char symbol[10];
    uint32_t num_bids;
    uint32_t num_asks;
    L2Entry bids[5]; // Top 5 bids
    L2Entry asks[5]; // Top 5 asks
};

struct ExecutionReport {
    MessageHeader header;
    uint64_t client_order_id;
    uint64_t execution_id;
    char symbol[10];
    uint8_t side; // 0=Buy, 1=Sell
    double price;
    uint64_t quantity;
    uint64_t filled_quantity;
    uint8_t status; // 0=New, 1=Partially Filled, 2=Filled, 3=Canceled, 4=Rejected
};

struct SubscriptionRequest {
    MessageHeader header;
    char symbol[10];
    uint8_t is_subscribe; // 0=Unsubscribe, 1=Subscribe
};

struct TradeUpdate {
    MessageHeader header;
    char symbol[10];
    double price;
    uint64_t quantity;
    uint64_t timestamp; // Epoch time in milliseconds
    uint8_t make_side;  // Who is the maker? 0=Buy, 1=Sell
};

struct OrderCancelRequest {
    MessageHeader header;
    uint64_t client_order_id;
    char symbol[10];
    int side; // 0=Buy, 1=Sell
};

#pragma pack(pop)
