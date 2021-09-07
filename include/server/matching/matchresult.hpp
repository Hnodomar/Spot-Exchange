#ifndef MATCH_RESULT_HPP
#define MATCH_RESULT_HPP

#include "fill.hpp"

namespace server {
namespace matching {
using Fill = ::info::Fill;
class MatchResult {
public:
    void addFill(int64_t timestamp, uint64_t order_filled_id, 
    uint64_t opposing_order_id, uint16_t fill_qty, uint8_t full_fill) {
        fills_.emplace_back(
            timestamp, order_filled_id, opposing_order_id, fill_qty, full_fill
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
private:
    std::vector<Fill> fills_;
    bool order_filled_ = false;
};
}
}

#endif
