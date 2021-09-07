#ifndef FILL_HPP
#define FILL_HPP

#include <cstdint>

// POD-ish struct for fillupdates
namespace info {
struct Fill {
    Fill(int64_t timestamp, uint64_t ticker, uint64_t order_id, 
    uint64_t price, uint16_t fill_qty, uint8_t full_fill)
    : timestamp(timestamp), ticker(ticker), order_id(order_id),
      price(price), fill_qty(fill_qty), full_fill(full_fill)
    {}
    int64_t timestamp;
    uint64_t ticker;
    uint64_t order_id;
    uint64_t price;
    uint16_t fill_qty;
    uint8_t full_fill;
};
}

#endif
