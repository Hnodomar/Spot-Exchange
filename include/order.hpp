#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdint>
#include <string>

#include "exception.hpp"

namespace tradeorder {
constexpr uint8_t HEADER_LEN = 2;
constexpr uint8_t MAX_BODY_LEN = 20;

class Order {
public: 
    //Order(uint64_t price, uint16_t quantity, uint8_t side):
    //    side_(side), price_(price), initial_quantity_(quantity),
    //    current_quantity_(quantity)
    //{}
    Order(uint8_t* buffer) {
        
    }
    uint8_t getSide() const {return side_;}
    uint64_t getPrice() const {return price_;}
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
    const std::string getUsername() {
        return std::string(reinterpret_cast<char*>(username_));
    }
    uint64_t getOrderID() {
        return order_id_;
    }
    uint64_t getTicker() {
        return ticker_;
    }
private:
    uint8_t side_;
    uint64_t price_;
    uint64_t order_id_ = 0;
    uint64_t ticker_ = 0;
    uint16_t initial_quantity_;
    uint16_t current_quantity_;
    uint8_t username_[20] = {0};
};

}
#endif
