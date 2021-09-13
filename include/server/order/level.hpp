#ifndef LEVEL_HPP
#define LEVEL_HPP

#include "limit.hpp"

namespace server {
namespace tradeorder {
struct Level {
    uint16_t getLevelOrderCount() {
        uint16_t cnt = 0;
        Limit* headptr = head;
        while (headptr != nullptr) {
            if (headptr->order.getCurrQty() != 0)
                ++cnt;
            headptr = headptr->next_limit;
        }
        return cnt;
    }
    uint32_t getLevelOrderQuantity() {
        uint32_t order_qty = 0;
        Limit* headptr = head;
        while (headptr != nullptr) {
            order_qty += headptr->order.getCurrQty();
            headptr = headptr->next_limit;
        }
        return order_qty;
    }
    Level(uint64_t price): price(price) {}
    Limit* head = nullptr;
    Limit* tail = nullptr;
    uint8_t is_buy_side = 0;
    uint64_t price = 0;
};
}
}

#endif
