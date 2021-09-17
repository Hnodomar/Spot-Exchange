#ifndef ORDERBOOK_MANAGER_HPP
#define ORDERBOOK_MANAGER_HPP

#include <unordered_map>
#include <cstring>
#include <utility>

#include "orderbook.hpp"

namespace server {
namespace tradeorder {
using SubscribeResult = std::pair<bool, OrderBook&>;
using order_id = uint64_t; using ticker = uint64_t;
class OrderBookManager {
public:
    static void addOrder(::tradeorder::Order& order);
    static void modifyOrder(const info::ModifyOrder& modify_order);
    static void cancelOrder(const info::CancelOrder& cancel_order);
    static bool createOrderBook(const uint64_t ticker);
    static bool createOrderBook(const std::string& ticker);
    static SubscribeResult subscribe(const uint64_t ticker);
    static SubscribeResult subscribe(const std::string& ticker);
    static uint64_t numOrderBooks() {return orderbooks_.size();}
private:
    static ticker convertStrToTicker(const std::string& input);
    static std::unordered_map<ticker, OrderBook> orderbooks_;
};
}
}
#endif
