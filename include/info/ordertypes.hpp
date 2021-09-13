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
    const uint64_t order_id;
    const uint64_t user_id;
    const uint64_t ticker;
};

struct ModifyOrder : public OrderCommon {
    ModifyOrder(uint8_t is_buy_side, uint64_t price, uint32_t quantity,  OrderCommon common)
    : OrderCommon(common)
    , quantity(quantity)
    , is_buy_side(is_buy_side)
    , price(price)
    {}
    const uint32_t quantity;
    const uint8_t is_buy_side;
    const uint64_t price;
};

struct CancelOrder : public OrderCommon {
    CancelOrder(uint64_t order_id, uint64_t user_id, uint64_t ticker)
    : OrderCommon(order_id, user_id, ticker) 
    {}
};  
}
#endif
