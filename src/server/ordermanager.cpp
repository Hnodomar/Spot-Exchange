#include "ordermanager.hpp"

using namespace server::tradeorder;

void OrderManager::addOrder(::tradeorder::Order order) {
    auto itr = orderbooks_.find(order.getTicker());
    if (itr == orderbooks_.end()) {
        auto ret = orderbooks_.emplace(
            std::make_pair(order.getTicker(), OrderBook())
        );
        if (!ret.second)
            return;
        itr = ret.first;
    }
    itr->second.addOrder(order);
}
