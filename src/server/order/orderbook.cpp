#include "orderbook.hpp"

using namespace server::tradeorder;

OrderResult OrderBook::addOrder(tradeorder::Order&& order) {
    if (limitorders_.find(order.getOrderID()) != limitorders_.end()) {
        return OrderResult(info::RejectionReason::order_id_already_present);
    }
    Level* level = nullptr;
    OrderResult order_result;
    if (order.isBuySide()) {
        order_result.match_result = MatchBids(order, bids_, limitorders_);
        level = &getSideLevel(order.getPrice(), bids_);
    }
    else {
        order_result.match_result = MatchAsks(order, asks_, limitorders_);
        level = &getSideLevel(order.getPrice(), asks_);
    }
    if (order_result.match_result.orderCompletelyFilled()) {
        return order_result;
    }
    auto limitr = limitorders_.emplace(order.getOrderID(), Limit(order));
    if (!limitr.second) {
        throw EngineException(
            "Unable to emplace new order in limitorder book, Book ID: "
            + std::to_string(ticker_) + " Order ID: " + std::to_string(order.getOrderID())
        );
    }
    Limit& limit = limitr.first->second;
    if (level->head == nullptr) {
        level->head = &limit;
        level->tail = &limit;
    }
    else {
        Limit* limit_temp = level->tail;
        level->tail = &limit;
        limit.prev_limit = limit_temp;
        limit_temp->next_limit = &limit;
    }
    return order_result;
}

OrderResult OrderBook::modifyOrder(const info::ModifyOrder& modify_order) {
    auto itr = limitorders_.find(modify_order.order_id);
    if (itr == limitorders_.end()) {
        return OrderResult(info::RejectionReason::order_not_found);
    }
    auto& limit = itr->second;
    if (limit.order.isBuySide() != modify_order.is_buy_side) {
        return OrderResult(info::RejectionReason::modify_wrong_side);
    }
    limit.order.decreaseQty(modify_order.quantity);
    return OrderResult(); //return modify order ack
}

OrderResult OrderBook::cancelOrder(const info::CancelOrder& cancel_order) {
    auto itr = limitorders_.find(cancel_order.order_id);
    if (itr == limitorders_.end()) {
        return OrderResult(info::RejectionReason::order_not_found);
    }
    limitorders_.erase(itr);
    return OrderResult(); // return cancel order ack
}

template<typename Side>
Level& OrderBook::getSideLevel(const uint64_t price, Side sidebook) {
    auto lvlitr = sidebook.find(price);
    if (lvlitr == sidebook.end()) {
        auto ret = sidebook.emplace(price, Level());
        if (!ret.second) {
            // error
        }
        lvlitr = ret.first;
    }
    return lvlitr->second;
}
