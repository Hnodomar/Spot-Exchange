#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstdint>
#include <map>
#include "order.hpp"

namespace server {
namespace tradeorder {
class OrderBook {
public:
    OrderBook() {
        
    }
private:
    uint64_t ticker_;
    using price = uint64_t; using size = uint64_t;
    std::map<price, size> bids_, asks_;
};
}
}
#endif
