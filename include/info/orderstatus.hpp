#ifndef ORDER_STATUS_HPP
#define ORDER_STATUS_HPP

#include <cstdint>

#include "ordertypes.hpp"

namespace info {

struct AddOrderStatus {
    AddOrderStatus(
        const uint64_t timestamp,
        const uint64_t price,
        const uint32_t quantity,
        const uint8_t is_buy_side,
        const uint64_t order_id,
        const uint64_t ticker,
        const uint64_t user_id
    )
    : timestamp(timestamp)
    , price(price)
    , quantity(quantity)
    , is_buy_side(is_buy_side)
    , order_id(order_id)
    , ticker(ticker)
    , user_id(user_id)
    {}
    AddOrderStatus()
    : timestamp(0)
    , price(0)
    , quantity(0)
    , is_buy_side(0)
    , order_id(0)
    , ticker(0)
    , user_id(0)
    {}
    const uint64_t timestamp;
    const uint64_t price;
    const uint64_t order_id;
    const uint64_t ticker;
    const uint64_t user_id;
    const uint32_t quantity;
    const uint8_t is_buy_side;
};

struct ModifyOrderStatus {
    int64_t timestamp;
    uint64_t price;
    uint32_t quantity;
    uint8_t is_buy_side;
};

struct CancelOrderStatus : public CancelOrder {
    CancelOrderStatus(const uint64_t timestamp, CancelOrder cancel_order)
     : CancelOrder(cancel_order)
     , timestamp(timestamp)
    {}
    CancelOrderStatus()
     : CancelOrder(0, 0, 0)
     , timestamp(0)
    {}
    const int64_t timestamp;
};
}

#endif
