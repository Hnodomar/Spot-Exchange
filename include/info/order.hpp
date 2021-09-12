#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdint>
#include <string>

#include "exception.hpp"
#include "ordertypes.hpp"

namespace tradeorder {
class Order : private info::OrderCommon {
public: 
    Order(const uint8_t is_buy_side, uint64_t price,
    uint32_t quantity, info::OrderCommon common)
        : info::OrderCommon(common)
        , is_buy_side_(is_buy_side)
        , price_(price)
        , initial_quantity_(quantity)
        , current_quantity_(quantity)
    {}
    uint8_t isBuySide() const {return is_buy_side_;}
    uint64_t getPrice() const {return price_;}
    uint64_t getOrderID() const {return order_id;}
    uint64_t getUserID() const {return user_id;}
    uint64_t getTicker() const {return ticker;}
    uint32_t getInitQty() const {return initial_quantity_;}
    uint32_t getCurrQty() const {return current_quantity_;}
    void increaseQty(uint32_t qty_delta) {current_quantity_ += qty_delta;}
    void decreaseQty(uint32_t qty_delta) {
        if (current_quantity_ < qty_delta) {
            throw EngineException(
                "Quantity delta > current quantity, Order ID: " + 
                std::to_string(getOrderID())
            );
        }
        current_quantity_ -= qty_delta;
    }
private:
    const uint8_t is_buy_side_;
    uint64_t price_;
    uint32_t initial_quantity_;
    uint32_t current_quantity_;
};

}
#endif
