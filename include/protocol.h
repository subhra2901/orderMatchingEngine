#pragma once
#include <cstdint>

#pragma pack(push, 1)

//Message Type
enum class MessageType : uint8_t {
    LOGIN_REQUEST = 'L',
    LOGIN_RESPONSE = 'R',
    NEW_ORDER = 'N',
    EXECUTION_REPORT = 'E',
    ClIENT_DISCONNECT = 'X' 
};

//Header for all messages
struct MessageHeader {
    uint16_t seq_num; 
    MessageType type;
    uint16_t msg_len;
};

//CLIENT -> SERVER: Login Request
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

//SERVER -> CLIENT: Login Response
struct LoginResponse {
    MessageHeader header;
    uint8_t status; // 0=Fail, 1=Success
    char message[50]; // Optional message
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
    uint8_t status; // 0=New, 1=Partially Filled, 2=Filled, 3=Canceled
};

#pragma pack(pop)
