#ifndef FILL_HPP
#define FILL_HPP

#include <cstdint>
#include <string>

// POD-ish struct for fillupdates
namespace info {
struct Fill {
    Fill(int64_t timestamp, uint64_t ticker, uint64_t order_id, 
    uint64_t price, uint16_t fill_qty, uint8_t full_fill,
    uint64_t user_id)
    : timestamp(timestamp), ticker(ticker), order_id(order_id),
      price(price), fill_qty(fill_qty), full_fill(full_fill),
      user_id(user_id)
    {}
    const int64_t timestamp;
    const uint64_t ticker;
    const uint64_t order_id;
    const uint64_t price;
    const uint16_t fill_qty;
    const uint8_t full_fill;
    const uint64_t user_id;
};
}

#endif
