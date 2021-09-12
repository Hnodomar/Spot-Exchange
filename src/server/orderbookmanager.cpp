#include "orderbookmanager.hpp"

using namespace server::tradeorder;

OrderResult OrderBookManager::addOrder(::tradeorder::Order&& order) {
    auto itr = orderbooks_.find(order.getTicker());
    if (itr == orderbooks_.end()) {
        return OrderResult(info::RejectionReason::orderbook_not_found);
    }
    return itr->second.addOrder(std::forward<::tradeorder::Order>(order));
}

OrderResult OrderBookManager::modifyOrder(const info::ModifyOrder& modify_order) {
    if (modify_order.quantity == 0)
        return OrderResult(info::RejectionReason::modification_trivial);
    auto itr = orderbooks_.find(modify_order.ticker);
    if (itr == orderbooks_.end()) {
        return OrderResult(info::RejectionReason::orderbook_not_found);
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
    if (itr == orderbooks_.end())
        return false;
    auto emplace_itr = orderbooks_.emplace(ticker, OrderBook());
    if (emplace_itr.second)
        return true;
    return false;
}

bool OrderBookManager::createOrderBook(const std::string& ticker) {
    if (ticker.length() > 8)   
        return false;
    std::size_t len = ticker.length();
    char arr[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    strncpy(arr, ticker.data(), len);
    return createOrderBook(*reinterpret_cast<uint64_t*>(arr));
}
