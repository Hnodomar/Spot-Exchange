#include "orderbook.hpp"

using namespace server::tradeorder;

OrderResult OrderBook::addOrder(tradeorder::Order& order) {
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
    if (order_result.match_result->orderCompletelyFilled()) {
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
    limit.current_level = level;
    populateNewOrderStatus(order, order_result);
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
    OrderResult order_result;
    populateModifyOrderStatus(modify_order, order_result);
    if (limit.order.getCurrQty() < modify_order.quantity)
        limit.order.increaseQty(modify_order.quantity + limit.order.getCurrQty());
    else
        limit.order.decreaseQty(limit.order.getCurrQty() - modify_order.quantity);
    return order_result; //return modify order ack
}

OrderResult OrderBook::cancelOrder(const info::CancelOrder& cancel_order) {
    auto itr = limitorders_.find(cancel_order.order_id);
    if (itr == limitorders_.end()) {
        return OrderResult(info::RejectionReason::order_not_found);
    }
    Limit& limit = itr->second;
    if (limit.next_limit == nullptr) {
        limit.current_level->tail = limit.prev_limit;
        limit.prev_limit->next_limit = nullptr;
    }
    else if (limit.prev_limit == nullptr) {
        limit.current_level->head = limit.next_limit;
        limit.next_limit->prev_limit = nullptr;
    }
    else {
        limit.next_limit->prev_limit = limit.prev_limit;
        limit.prev_limit->next_limit = limit.next_limit;
    }
    limitorders_.erase(itr);
    OrderResult cancel_result;
    populateCancelOrderStatus(cancel_order, cancel_result);
    return cancel_result; // return cancel order ack
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

void OrderBook::populateNewOrderStatus(const tradeorder::Order& order, 
OrderResult& order_result) const {
    auto order_status = order_result.orderstatus.new_order_status;
    order_status.is_buy_side = order.isBuySide();
    order_status.order_id = order.getOrderID();
    order_status.ticker = order.getTicker();
    order_status.user_id = order.getUserID();
    order_status.quantity = order.getCurrQty();
    order_status.price = order.getPrice();
    order_status.timestamp = util::getUnixTimestamp();
    order_result.order_status_present = info::OrderStatusPresent::NewOrderPresent;
}

void OrderBook::populateModifyOrderStatus(const info::ModifyOrder& order, 
OrderResult& order_result) const {
    auto order_status = order_result.orderstatus.modify_order_status;
    order_status.is_buy_side = order.is_buy_side;
    order_status.order_id = order.order_id;
    order_status.ticker = order.ticker;
    order_status.user_id = order.user_id;
    order_status.quantity = order.quantity;
    order_status.price = order.price;
    order_status.timestamp = util::getUnixTimestamp();
    order_result.order_status_present = info::OrderStatusPresent::ModifyOrderPresent;
}

void OrderBook::populateCancelOrderStatus(const info::CancelOrder& order, 
OrderResult& order_result) const {
    auto order_status = order_result.orderstatus.cancel_order_status;
    order_status.order_id = order.order_id;
    order_status.user_id = order.user_id;
    order_status.ticker = order.ticker;
    order_status.timestamp = util::getUnixTimestamp();
    order_result.order_status_present = info::OrderStatusPresent::CancelOrderPresent;
}
