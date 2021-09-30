#ifndef ORDERBOOK_MANAGER_HPP
#define ORDERBOOK_MANAGER_HPP

#include <unordered_map>
#include <cstring>
#include <thread>
#include <utility>
#include <shared_mutex>

#ifndef TEST_BUILD
#include "marketdatadispatcher.hpp"
#else
namespace rpc {class MarketDataDispatcher;}
struct Rejection {Rejection(const uint8_t&){}};
#endif
#include "orderbook.hpp"
#include "logger.hpp"

namespace server {
namespace tradeorder {
using SubscribeResult = std::pair<bool, OrderBook&>;
using order_id = uint64_t; using ticker = uint64_t;
class OrderBookManager {
public:
    OrderBookManager(rpc::MarketDataDispatcher* md_dispatcher);
    static void addOrder(::tradeorder::Order& order);
    static void modifyOrder(const info::ModifyOrder& modify_order);
    static void cancelOrder(const info::CancelOrder& cancel_order);
    static bool createOrderBook(const uint64_t ticker);
    static bool createOrderBook(const std::string& ticker);
    static SubscribeResult subscribe(const uint64_t ticker);
    static SubscribeResult subscribe(const std::string& ticker);
    static uint64_t numOrderBooks() {return orderbooks_.size();}
    static void clearBooks() {OrderBookManager::orderbooks_.clear();}
private:
    static rpc::MarketDataDispatcher* marketdata_dispatcher_;
    static ticker convertStrToTicker(const std::string& input);
    static std::unordered_map<ticker, OrderBook> orderbooks_;
    template<typename OrderType>
    static void sendRejection(Rejection rejection, const OrderType& order);
};

template<typename OrderType>
inline void OrderBookManager::sendRejection(Rejection rejection, const OrderType& order) {
    #ifndef TEST_BUILD
    order.connection->sendRejection(
        rejection,
        order.user_id,
        order.order_id,
        order.ticker
    );
    #endif
}
}
}
#endif
