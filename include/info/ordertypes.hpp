#ifndef ORDER_TYPES_HPP
#define ORDER_TYPES_HPP

#include <cstdint>

namespace info {
struct OrderCommon {
    OrderCommon(uint64_t order_id, uint64_t user_id, uint64_t ticker)
    : order_id(order_id)
    , user_id(user_id)
    , ticker(ticker)
    {}
    uint64_t order_id;
    uint64_t user_id;
    uint64_t ticker;
};

struct ModifyOrder : public OrderCommon {
    ModifyOrder(uint32_t quantity, uint8_t is_buy_side, uint64_t price,
    uint64_t order_id, uint64_t user_id, uint64_t ticker)
    : OrderCommon(order_id, user_id, ticker)
    , quantity(quantity)
    , is_buy_side(is_buy_side)
    , price(price)
    {}
    uint32_t quantity;
    uint8_t is_buy_side;
    int64_t price;
};

struct CancelOrder : public OrderCommon {
    CancelOrder(uint64_t order_id, uint64_t user_id, uint64_t ticker)
    : OrderCommon(order_id, user_id, ticker) 
    {}
};  
}
#endif
