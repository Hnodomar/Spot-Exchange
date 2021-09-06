#ifndef LIMIT_HPP
#define LIMIT_HPP

#include "order.hpp"

namespace server {
namespace tradeorder {
struct Level;
struct Limit { 
    Limit(::tradeorder::Order order, Level& current_level):
        order(order), current_level(current_level)
    {}
    uint64_t timestamp;
    ::tradeorder::Order order;
    Level& current_level;
    Limit* next_limit = nullptr;
    Limit* prev_limit = nullptr;
};
}
}

#endif
