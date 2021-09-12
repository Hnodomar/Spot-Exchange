#ifndef ORDER_STATUS_HPP
#define ORDER_STATUS_HPP

#include <cstdint>

namespace info {
struct OrderStatusCommon {
    OrderStatusCommon(uint64_t order_id, uint64_t user_id, uint64_t ticker)
    : order_id(order_id)
    , ticker(ticker)
    , user_id(user_id)
    {}
    uint64_t order_id;
    uint64_t ticker;
    uint64_t user_id;
};

struct NewOrderStatus : public OrderStatusCommon {
    NewOrderStatus(int64_t timestamp, uint64_t price, 
    uint32_t quantity, uint8_t is_buy_side, OrderStatusCommon common)
    : OrderStatusCommon(common)
    , timestamp(timestamp)
    , price(price)
    , quantity(quantity)
    , is_buy_side(is_buy_side) 
    {}
    int64_t timestamp;
    uint64_t price;
    uint32_t quantity;
    uint8_t is_buy_side;
};

struct ModifyOrderStatus : public OrderStatusCommon {
    ModifyOrderStatus(int64_t timestamp, uint64_t price, 
    uint32_t quantity, uint8_t is_buy_side, OrderStatusCommon common)
    : OrderStatusCommon(common)
    , timestamp(timestamp)
    , price(price)
    , quantity(quantity)
    , is_buy_side(is_buy_side) 
    {}
    int64_t timestamp;
    uint64_t price;
    uint32_t quantity;
    uint8_t is_buy_side;
};

struct CancelOrderStatus : public OrderStatusCommon {
    CancelOrderStatus(const uint64_t timestamp, OrderStatusCommon common)
    : OrderStatusCommon(common), timestamp(timestamp)
    {}
    int64_t timestamp;
};
}

#endif
