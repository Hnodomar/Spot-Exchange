#ifndef ORDER_TYPES_HPP
#define ORDER_TYPES_HPP

#include <cstdint>

// POD structs for cheap C-style casts
// standard prohibits compiler reordering of struct memory layout
// for the sake of C compatibility
namespace info {
struct AddOrder {
    uint64_t order_id;
    uint8_t side;
    uint64_t price;
    uint64_t ticker;
    uint16_t quantity;
    uint8_t username[20];
};

struct ModifyOrder {
    uint64_t order_id;
    uint16_t quantity;
    uint64_t ticker;
    uint8_t addOrRemove;
};

struct CancelOrder {
    uint64_t order_id;
    uint64_t ticker;
};  
}
#endif
