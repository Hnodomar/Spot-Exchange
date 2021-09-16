#ifndef LIMIT_HPP
#define LIMIT_HPP

#include "order.hpp"

namespace server {
namespace tradeorder {
struct Level;
struct Limit { 
    Limit(::tradeorder::Order order):
        order(order)
    {}
    Limit() = default;
    ~Limit() {}
    uint64_t timestamp;
    ::tradeorder::Order order;
    Limit* next_limit = nullptr;
    Limit* prev_limit = nullptr;
    Level* current_level = nullptr;
};
}
}

#endif
