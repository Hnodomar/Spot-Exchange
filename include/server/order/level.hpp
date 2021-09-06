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
    Limit* head = nullptr;
    Limit* tail = nullptr;
    uint64_t price = 0;
};
}
}

#endif
