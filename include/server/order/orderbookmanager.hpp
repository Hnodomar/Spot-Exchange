#ifndef ORDERBOOK_MANAGER_HPP
#define ORDERBOOK_MANAGER_HPP

#include <unordered_map>
#include <cstring>

#include "orderbook.hpp"
#include "orderresult.hpp"

namespace server {
namespace tradeorder {
class OrderBookManager {
public:
    OrderResult addOrder(::tradeorder::Order&& order);
    OrderResult modifyOrder(const info::ModifyOrder& modify_order);
    OrderResult cancelOrder(const info::CancelOrder& cancel_order);
    bool createOrderBook(const uint64_t ticker);
    bool createOrderBook(const std::string& ticker);
private:
    using order_id = uint64_t; using ticker = uint64_t;
    using book_index = uint64_t;
    std::unordered_map<ticker, OrderBook> orderbooks_;
};
}
}
#endif
