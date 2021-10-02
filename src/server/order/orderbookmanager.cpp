#include "orderbookmanager.hpp"
#include <iostream>

using namespace server::tradeorder;

std::unordered_map<ticker, OrderBook> OrderBookManager::orderbooks_;
rpc::MarketDataDispatcher* OrderBookManager::marketdata_dispatcher_;

OrderBookManager::OrderBookManager(rpc::MarketDataDispatcher* md_dispatcher) {
    OrderBookManager::marketdata_dispatcher_ = md_dispatcher;
}

void OrderBookManager::addOrder(::tradeorder::Order& order) {
    auto itr = OrderBookManager::orderbooks_.find(order.getTicker());
    if (itr == orderbooks_.end()) {
        #ifndef TEST_BUILD
        order.connection_->sendRejection(
            static_cast<Rejection>(ORDERBOOK_NOT_FOUND),
            order.getUserID(),
            order.getOrderID(),
            order.getTicker()
        );
        #endif
        return;
    }
    itr->second.addOrder(order);
}

void OrderBookManager::modifyOrder(const info::ModifyOrder& modify_order) {
    if (modify_order.quantity == 0) {
        sendRejection(static_cast<Rejection>(MODIFICATION_TRIVIAL), modify_order);
        return;
    }
    auto itr = OrderBookManager::orderbooks_.find(modify_order.ticker);
    if (itr == OrderBookManager::orderbooks_.end()) {
        sendRejection(static_cast<Rejection>(ORDERBOOK_NOT_FOUND), modify_order);
        return;
    }
    itr->second.modifyOrder(modify_order);
}

void OrderBookManager::cancelOrder(const info::CancelOrder& cancel_order) {
    auto itr = OrderBookManager::orderbooks_.find(cancel_order.ticker);
    if (itr == OrderBookManager::orderbooks_.end()) {
        sendRejection(static_cast<Rejection>(ORDERBOOK_NOT_FOUND), cancel_order);
        return;
    }
    itr->second.cancelOrder(cancel_order);
}

bool OrderBookManager::createOrderBook(const uint64_t ticker) {
    auto itr = OrderBookManager::orderbooks_.find(ticker);
    if (itr != OrderBookManager::orderbooks_.end()) {
        logging::Logger::Log(
            logging::LogType::Warning, 
            util::getLogTimestamp(), 
            "Failed to create orderbook", util::ShortString(ticker), 
            "already exists"
        );
        return false;
    }
    auto emplace_itr = OrderBookManager::orderbooks_.emplace(ticker, OrderBook(marketdata_dispatcher_));
    if (emplace_itr.second) {
        logging::Logger::Log(
            logging::LogType::Debug, 
            util::getLogTimestamp(), 
            "Successfully created orderbook", util::ShortString(ticker)
        );
        return true;
    }
    logging::Logger::Log(
        logging::LogType::Warning, 
        util::getLogTimestamp(), 
        "Failed to create orderbook", util::ShortString(ticker), 
        "emplace error"
    );
    return false;
}

bool OrderBookManager::createOrderBook(const std::string& ticker) {
    return createOrderBook(convertStrToTicker(ticker));
}

SubscribeResult OrderBookManager::subscribe(const uint64_t ticker) {
    auto itr = OrderBookManager::orderbooks_.find(ticker);
    if (itr == OrderBookManager::orderbooks_.end()) {
        OrderBook dangler;
        logging::Logger::Log(
            logging::LogType::Debug, 
            util::getLogTimestamp(), 
            "Failed to subscribe to orderbook", 
            util::ShortString(ticker)
        );
        return {false, dangler};
    }
    logging::Logger::Log(
        logging::LogType::Debug, 
        util::getLogTimestamp(), 
        "Successfully subscribed to orderbook", util::ShortString(ticker)
    );
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
