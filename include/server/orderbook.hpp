#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstdint>
#include <map>
#include "order.hpp"
#include "level.hpp"

namespace server {
namespace tradeorder {
class OrderBook {
public:
    OrderBook() {
        
    }
    void addOrder(::tradeorder::Order&& order) {
        std::map<price, Level>& sidebook = order.getSide() == 'B' ? bids_ : asks_;
        auto lvlitr = sidebook.find(order.getPrice());
        if (lvlitr == sidebook.end()) {
            auto ret = sidebook.emplace(order.getPrice(), Level());
            if (!ret.second)
                return;
            lvlitr = ret.first;
        }
        Level& level = lvlitr->second;
        auto limitr = limitorders_.emplace(order.getOrderID(), Limit(order, level));
        if (!limitr.second)
            return;
        Limit& limit = limitr.first->second;
        if (level.head == nullptr) {
            level.head = &limit;
            level.tail = &limit;
        }
        else {
            Limit* limit_temp = level.tail;
            level.tail = &limit;
            limit.prev_limit = limit_temp;
            limit_temp->next_limit = &limit;
        }
    }
    void modifyOrder() {

    }
private:
    uint64_t ticker_;
    using price = uint64_t; using size = uint64_t;
    std::map<price, Level> bids_, asks_;
    std::unordered_map<uint64_t, Limit> limitorders_;
};
}
}
#endif
