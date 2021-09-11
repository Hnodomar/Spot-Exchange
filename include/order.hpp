#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdint>
#include <string>

#include "exception.hpp"
#include "ordertypes.hpp"

namespace tradeorder {
constexpr uint8_t HEADER_LEN = 2;
constexpr uint8_t MAX_BODY_LEN = 20;

class Order {
public: 
    Order(const uint8_t is_buy_side, uint64_t price, const uint64_t order_id, 
        const uint64_t ticker, uint32_t quantity, const uint64_t user_id)
        : is_buy_side_(is_buy_side)
        , price_(price)
        , order_id_(order_id)
        , ticker_(ticker)
        , initial_quantity_(quantity)
        , current_quantity_(quantity)
        , user_id_(user_id) 
    {}
    uint8_t getSide() const {return is_buy_side_;}
    uint64_t getPrice() const {return price_;}
    uint64_t getOrderID() const {return order_id_;}
    uint64_t getTicker() const {return ticker_;}
    uint16_t getInitQty() const {return initial_quantity_;}
    uint16_t getCurrQty() const {return current_quantity_;}
    void increaseQty(uint16_t qty_delta) {current_quantity_ += qty_delta;}
    void decreaseQty(uint16_t qty_delta) {
        if (current_quantity_ < qty_delta) {
            throw EngineException(
                "Quantity delta > current quantity, Order ID: " + 
                std::to_string(getOrderID())
            );
        }
        current_quantity_ -= qty_delta;
    }
    const uint64_t getUserID() {
        return user_id_;
    }
private:
    const uint8_t is_buy_side_;
    uint64_t price_;
    const uint64_t order_id_;
    const uint64_t ticker_;
    uint16_t initial_quantity_;
    uint16_t current_quantity_;
    const uint64_t user_id_;
};

}
#endif
