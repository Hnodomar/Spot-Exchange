#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdint>
#include <string>

#include "exception.hpp"
#include "ordertypes.hpp"

namespace tradeorder {
class Order {
public: 
    Order(const uint8_t is_buy_side, OrderEntryStreamConnection* connection,
    uint64_t price, uint32_t quantity, info::OrderCommon common)
        : price_(price)
        , order_id(common.order_id)
        , user_id(common.user_id)
        , ticker(common.ticker)
        , connection_(connection)
        , initial_quantity_(quantity)
        , current_quantity_(quantity)
        , is_buy_side_(is_buy_side)
    {}
    Order(const info::ModifyOrder& mod)
        : price_(mod.price)
        , order_id(mod.order_id)
        , user_id(mod.user_id)
        , ticker(mod.ticker)
        , connection_(mod.connection)
        , initial_quantity_(mod.quantity)
        , current_quantity_(mod.quantity)
        , is_buy_side_(mod.is_buy_side)
    {}
    //Order() 
    //    : info::OrderCommon(0, 0, 0)
    //    , price_(0)
     //   , is_buy_side_(0)
    //{}
    Order() = default;
    Order(Order&) = default;
    uint8_t isBuySide() const {return is_buy_side_;}
    uint64_t getPrice() const {return price_;}
    OrderEntryStreamConnection* getConnection() {return connection_;};
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
    uint64_t price_;
    uint64_t order_id;
    uint64_t user_id;
    uint64_t ticker;
    OrderEntryStreamConnection* connection_ = nullptr;
    uint32_t initial_quantity_;
    uint32_t current_quantity_;
    uint8_t is_buy_side_;
};

}
#endif
