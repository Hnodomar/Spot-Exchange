#ifndef ORDER_TYPES_HPP
#define ORDER_TYPES_HPP

#include <cstdint>

class OrderEntryStreamConnection;

namespace info {
struct OrderCommon {
    OrderCommon(uint64_t order_id, uint64_t user_id, uint64_t ticker)
    : order_id(order_id)
    , user_id(user_id)
    , ticker(ticker)
    {}
    OrderCommon(OrderCommon&) = default;
    const uint64_t order_id;
    const uint64_t user_id;
    const uint64_t ticker;
};

struct ModifyOrder : public OrderCommon {
    ModifyOrder(uint8_t is_buy_side, OrderEntryStreamConnection* connection,
    uint64_t price, uint32_t quantity, OrderCommon common)
    : OrderCommon(common)
    , price(price)
    , connection(connection)
    , quantity(quantity)
    , is_buy_side(is_buy_side)
    {}
    const uint64_t price;
    OrderEntryStreamConnection* connection;
    const uint32_t quantity;
    const uint8_t is_buy_side;
};

struct CancelOrder : public OrderCommon {
    CancelOrder(uint64_t order_id, uint64_t user_id, uint64_t ticker,
    OrderEntryStreamConnection* conn)
    : OrderCommon(order_id, user_id, ticker)
    , connection(conn)
    {}
    CancelOrder(const ModifyOrder& mod)
    : OrderCommon(mod.order_id, mod.user_id, mod.ticker)
    , connection(mod.connection)
    {}
    CancelOrder(const CancelOrder& rhs)
     : OrderCommon(rhs.order_id, rhs.user_id, rhs.ticker)
    {}
    CancelOrder(CancelOrder&&) = default;
    OrderEntryStreamConnection* connection;
};  
}
#endif
