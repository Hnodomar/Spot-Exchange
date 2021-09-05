#include "ordermanager.hpp"

using namespace server::tradeorder;

void OrderManager::addOrder(::tradeorder::Order order) {
    if (tickers_.find(order.getTicker()) == tickers_.end()) {
        orderbooks_.emplace_back(OrderBook());
        tickers_.insert(order.getTicker(), orderbooks_.size() - 1);
    }
    orders_.insert(std::make_pair(order.getOrderID(), order));
}
