#include "orderbookmanager.hpp"
#include <iostream>

using namespace server::tradeorder;

void OrderBookManager::addOrder(::tradeorder::Order& order) {
    auto itr = orderbooks_.find(order.getTicker());
    if (itr == orderbooks_.end()) {
        order.getConnection()->sendRejection(
            static_cast<Rejection>(ORDERBOOK_NOT_FOUND),
            order.getUserID(),
            order.getOrderID(),
            order.getTicker()
        );
        return;
    }
    (itr->second.*add_order[order.isBuySide()])(order);
}

void OrderBookManager::modifyOrder(const info::ModifyOrder& modify_order) {
    if (modify_order.quantity == 0) {
        modify_order.connection->sendRejection(
            static_cast<Rejection>(MODIFICATION_TRIVIAL),
            modify_order.user_id,
            modify_order.order_id,
            modify_order.ticker
        );
        return;
    }
    auto itr = orderbooks_.find(modify_order.ticker);
    if (itr == orderbooks_.end()) {
        modify_order.connection->sendRejection(
            static_cast<Rejection>(ORDERBOOK_NOT_FOUND),
            modify_order.user_id,
            modify_order.order_id,
            modify_order.ticker
        );
        return;
    }
    itr->second.modifyOrder(modify_order);
}

void OrderBookManager::cancelOrder(const info::CancelOrder& cancel_order) {
    auto itr = orderbooks_.find(cancel_order.ticker);
    if (itr == orderbooks_.end()) {
        cancel_order.connection->sendRejection(
            static_cast<Rejection>(ORDERBOOK_NOT_FOUND),
            cancel_order.user_id,
            cancel_order.order_id,
            cancel_order.ticker
        );
        return;
    }
    itr->second.cancelOrder(cancel_order);
}

bool OrderBookManager::createOrderBook(const uint64_t ticker) {
    auto itr = orderbooks_.find(ticker);
    if (itr != orderbooks_.end())
        return false;
    auto emplace_itr = orderbooks_.emplace(ticker, OrderBook());
    if (emplace_itr.second)
        return true;
    return false;
}

bool OrderBookManager::createOrderBook(const std::string& ticker) {
    return createOrderBook(convertStrToTicker(ticker));
}

SubscribeResult OrderBookManager::subscribe(const uint64_t ticker) {
    auto itr = orderbooks_.find(ticker);
    if (itr == orderbooks_.end()) {
        OrderBook dangler;
        return {false, dangler};
    }
    return {true, orderbooks_.find(ticker)->second};
}

SubscribeResult OrderBookManager::subscribe(const std::string& ticker) {
    return subscribe(convertStrToTicker(ticker));
}

ticker OrderBookManager::convertStrToTicker(const std::string& input) {
    std::size_t len = input.length();
    if (len > 8) len = 8;
    char arr[8] = {0};
    strncpy(arr, input.data(), len);
    return *reinterpret_cast<uint64_t*>(arr);
}
