#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdint>
#include <string>

#include "exception.hpp"
#include "ordertypes.hpp"

namespace tradeorder {
class Order {
public: 
    Order(uint8_t is_buy_side, OrderEntryStreamConnection* connection,
    uint64_t price, uint32_t quantity, info::OrderCommon common)
        : connection_(connection)
        , price_(price)
        , order_id(common.order_id)
        , user_id(common.user_id)
        , ticker(common.ticker)
        , initial_quantity_(quantity)
        , current_quantity_(quantity)
        , is_buy_side_(is_buy_side)
    {}
    Order(const info::ModifyOrder& mod)
        : connection_(mod.connection)
        , price_(mod.price)
        , order_id(mod.order_id)
        , user_id(mod.user_id)
        , ticker(mod.ticker)
        , initial_quantity_(mod.quantity)
        , current_quantity_(mod.quantity)
        , is_buy_side_(mod.is_buy_side)
    {}
    Order(const Order& order) 
        : connection_(order.connection_)
        , price_(order.getPrice())
        , order_id(order.getOrderID())
        , user_id(order.getUserID())
        , ticker(order.getTicker())
        , initial_quantity_(order.getInitQty())
        , current_quantity_(order.getCurrQty())
        , is_buy_side_(order.isBuySide())
    {}
    Order() {
        
    }
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
    OrderEntryStreamConnection* connection_;
private:
    uint64_t price_;
    uint64_t order_id;
    uint64_t user_id;
    uint64_t ticker;
    uint32_t initial_quantity_;
    uint32_t current_quantity_;
    uint8_t is_buy_side_;
};

}
#endif
