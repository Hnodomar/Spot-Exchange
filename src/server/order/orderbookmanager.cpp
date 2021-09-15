#include "orderbookmanager.hpp"
#include <iostream>

using namespace server::tradeorder;

AddOrderResult OrderBookManager::addOrder(tradeorder::Order& order) {
    auto itr = orderbooks_.find(order.getTicker());
    if (itr == orderbooks_.end()) {
        return info::AddOrderResult(info::AddOrderRejectionReason::orderbook_not_found);
    }
    return (*(itr->second.add_order[order.isBuySide()]))(order);
}

ModifyOrderResult OrderBookManager::modifyOrder(const info::ModifyOrder& modify_order) {
    if (modify_order.quantity == 0)
        return ModifyOrderResult(ModifyRejectionReason::modification_trivial);
    auto itr = orderbooks_.find(modify_order.ticker);
    if (itr == orderbooks_.end())
        return ModifyOrderResult(ModifyRejectionReason::orderbook_not_found);
    return itr->second.modifyOrder(modify_order);
}

CancelOrderResult OrderBookManager::cancelOrder(const info::CancelOrder& cancel_order) {
    auto itr = orderbooks_.find(cancel_order.ticker);
    if (itr == orderbooks_.end()) {
        return CancelOrderResult(info::CancelRejectionReason::orderbook_not_found);
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
