#ifndef MATCH_RESULT_HPP
#define MATCH_RESULT_HPP

#include "fill.hpp"

namespace server {
namespace matching {
using Fill = ::info::Fill;
class MatchResult {
public:
    void addFill(int64_t timestamp, uint64_t ticker, uint64_t order_id, 
    uint64_t price, uint16_t fill_qty, uint8_t full_fill, uint64_t user_id) {
        fills_.emplace_back(
            timestamp, ticker, order_id, price, fill_qty, full_fill, user_id
        );
    }
    bool orderCompletelyFilled() {
        return order_filled_;
    }
    void setOrderFilled() {
        order_filled_ = true;
    }
    uint numFills() {
        return fills_.size();
    }
    std::vector<Fill>& getFills() {
        return fills_;
    }
private:
    std::vector<Fill> fills_;
    bool order_filled_ = false;
};
}
}

#endif
