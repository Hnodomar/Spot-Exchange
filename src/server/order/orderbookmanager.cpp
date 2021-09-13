#include "orderbookmanager.hpp"
#include <iostream>

using namespace server::tradeorder;

OrderResult OrderBookManager::addOrder(tradeorder::Order& order) {
    auto itr = orderbooks_.find(order.getTicker());
    if (itr == orderbooks_.end()) {
        return OrderResult(info::RejectionReason::orderbook_not_found);
    }
    return itr->second.addOrder(order);
}

std::pair<OrderResult, OrderResult> OrderBookManager::modifyOrder(const info::ModifyOrder& modify_order) {
    if (modify_order.quantity == 0)
        return {OrderResult(info::RejectionReason::modification_trivial), OrderResult()};
    auto itr = orderbooks_.find(modify_order.ticker);
    if (itr == orderbooks_.end()) {
        return {OrderResult(info::RejectionReason::orderbook_not_found), OrderResult()};
    }
    return itr->second.modifyOrder(modify_order);
}

OrderResult OrderBookManager::cancelOrder(const info::CancelOrder& cancel_order) {
    auto itr = orderbooks_.find(cancel_order.ticker);
    if (itr == orderbooks_.end()) {
        return OrderResult(info::RejectionReason::orderbook_not_found);
    }
    return itr->second.cancelOrder(cancel_order);
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
