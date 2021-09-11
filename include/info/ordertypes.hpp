#ifndef ORDER_TYPES_HPP
#define ORDER_TYPES_HPP

#include <cstdint>

namespace info {
struct OrderCommon {
    uint64_t order_id;
    uint64_t user_id;
    uint64_t ticker;
};

struct ModifyOrder : public OrderCommon {
    uint32_t quantity;
    uint8_t is_buy_side;
    int64_t price;
};

struct CancelOrder : public OrderCommon {};  
}
#endif
