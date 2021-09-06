#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdint>
#include <string>
#include <cstring>

#include "exception.hpp"
#include "ordertypes.hpp"

namespace tradeorder {
constexpr uint8_t HEADER_LEN = 2;
constexpr uint8_t MAX_BODY_LEN = 20;

class Order {
public: 
    Order(AddOrder* addorder_pod): 
        side_(addorder_pod->side),
        price_(addorder_pod->price),
        order_id_(addorder_pod->order_id),
        ticker_(addorder_pod->ticker),
        initial_quantity_(addorder_pod->quantity),
        current_quantity_(addorder_pod->quantity) {
        std::memcpy(username_, addorder_pod->username, 20);
    }
    uint8_t getSide() const {return side_;}
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
    const std::string getUsername() {
        return std::string(reinterpret_cast<const char*>(username_));
    }
private:
    const uint8_t side_;
    uint64_t price_;
    const uint64_t order_id_;
    const uint64_t ticker_;
    uint16_t initial_quantity_;
    uint16_t current_quantity_;
    uint8_t username_[20] = {0};
};

}
#endif
