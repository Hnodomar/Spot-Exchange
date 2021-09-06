#ifndef FILL_HPP
#define FILL_HPP

#include <cstdint>

// POD-ish struct for fillupdates

struct Fill {
    uint64_t unix_time;
    uint64_t order_filled_id;
    uint64_t opposing_order_id;
    uint16_t fill_qty;
    uint8_t full_fill;
};

#endif
