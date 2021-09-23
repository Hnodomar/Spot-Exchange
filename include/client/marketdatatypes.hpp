#ifndef MARKET_DATA_TYPES_HPP
#define MARKET_DATA_TYPES_HPP

#include <cstdint>

// C-Style POD structs for casting UDP buffer

namespace client {
struct AddOrderData {
    int64_t timestamp;
    uint64_t order_id;
    uint64_t ticker;
    uint64_t price;
    int32_t quantity;
    uint8_t is_buy_side; // purposefully misaligned
    uint16_t book_index; // padding helps us worry less about cast
};

struct ModOrderData {
    int64_t timestamp;
    uint64_t order_id;
    int32_t quantity;
};

struct CancelOrderData {
    int64_t timestamp;
    uint64_t order_id;
};

struct FillOrderData {
    int64_t timestamp;
    uint64_t order_id;
    uint64_t ticker;
    uint64_t fill_id;
    int32_t quantity;
};

struct NotificationData {
    int64_t timestamp;
    uint8_t flag;
};
}

#endif
