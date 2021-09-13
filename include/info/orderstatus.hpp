#ifndef ORDER_STATUS_HPP
#define ORDER_STATUS_HPP

#include <cstdint>

namespace info {
struct OrderStatusCommon {
    uint64_t order_id;
    uint64_t ticker;
    uint64_t user_id;
};

struct NewOrderStatus : public OrderStatusCommon {
    int64_t timestamp;
    uint64_t price;
    uint32_t quantity;
    uint8_t is_buy_side;
};

struct ModifyOrderStatus : public OrderStatusCommon {
    int64_t timestamp;
    uint64_t price;
    uint32_t quantity;
    uint8_t is_buy_side;
};

struct CancelOrderStatus : public OrderStatusCommon {
    int64_t timestamp;
};
}

#endif
