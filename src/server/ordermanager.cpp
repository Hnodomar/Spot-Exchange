#include "ordermanager.hpp"

using namespace server::tradeorder;

OrderResult OrderManager::addOrder(::tradeorder::Order&& order) {
    auto itr = orderbooks_.find(order.getTicker());
    if (itr == orderbooks_.end()) {
        return OrderResult(info::RejectionReason::orderbook_not_found);
    }
    return itr->second.addOrder(std::forward<::tradeorder::Order>(order));
}

OrderResult OrderManager::modifyOrder(info::ModifyOrder& modify_order) {
    if (modify_order.quantity == 0)
        return OrderResult(info::RejectionReason::modification_trivial);
    auto itr = orderbooks_.find(modify_order.ticker);
    if (itr == orderbooks_.end()) {
        return OrderResult(info::RejectionReason::orderbook_not_found);
    }
    return itr->second.modifyOrder(modify_order);
}

OrderResult OrderManager::cancelOrder(info::CancelOrder& cancel_order) {
    auto itr = orderbooks_.find(cancel_order.ticker);
    if (itr == orderbooks_.end()) {
        return OrderResult(info::RejectionReason::orderbook_not_found);
    }
    return itr->second.cancelOrder(cancel_order);
}
