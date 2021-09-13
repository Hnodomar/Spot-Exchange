#include "orderbook.hpp"
#include <iostream>

using namespace server::tradeorder;

OrderResult OrderBook::addOrder(Order& order) {
    using server::matching::MatchResult;
    if (limitorders_.find(order.getOrderID()) != limitorders_.end()) {
        return OrderResult(info::RejectionReason::order_id_already_present);
    }
    Level* level = nullptr;
    OrderResult order_result;
    if (order.isBuySide()) {
        if (asks_.begin()->first <= order.getPrice()) {
            order_result.match_result = MatchAsks(order, asks_, limitorders_);
            if (orderFilled(order_result.match_result))
                return order_result;
        }
        level = &getSideLevel(order.getPrice(), bids_);
        level->is_buy_side = 1;
    }
    else {
        if (bids_.begin()->first <= order.getPrice()) {
            order_result.match_result = MatchBids(order, bids_, limitorders_);
            if (orderFilled(order_result.match_result))
                return order_result;
        }
        level = &getSideLevel(order.getPrice(), asks_);
        level->is_buy_side = 0;
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

std::pair<OrderResult, OrderResult> OrderBook::modifyOrder(const info::ModifyOrder& modify_order) {
    auto itr = limitorders_.find(modify_order.order_id);
    if (itr == limitorders_.end()) {
        return {OrderResult(info::RejectionReason::order_not_found), OrderResult()};
    }
    auto& limit = itr->second;
    if (limit.order.isBuySide() != modify_order.is_buy_side) {
        return {OrderResult(info::RejectionReason::modify_wrong_side), OrderResult()};
    }
    if (itr->second.order.getUserID() != modify_order.user_id) {
        return {OrderResult(info::RejectionReason::wrong_user_id), OrderResult()};
    }
    if (modifyOrderTrivial(modify_order, limit.order)) {
        return {OrderResult(info::RejectionReason::modification_trivial), OrderResult()};
    }
    if (modify_order.price == limit.order.getPrice()) {
        OrderResult order_result;
        populateModifyOrderStatus(modify_order, order_result);
        if (limit.order.getCurrQty() < modify_order.quantity)
            limit.order.increaseQty(modify_order.quantity + limit.order.getCurrQty());
        else
            limit.order.decreaseQty(limit.order.getCurrQty() - modify_order.quantity);
        return {order_result, OrderResult()};
    }
    else {
        Order modified_order(
            modify_order.is_buy_side,
            modify_order.price,
            modify_order.quantity,
            info::OrderCommon(
                modify_order.order_id,
                modify_order.user_id,
                modify_order.ticker
            )
        );
        info::CancelOrder original_order(
            modify_order.order_id,
            modify_order.user_id,
            modify_order.ticker
        );
        OrderResult cancel_result = cancelOrder(original_order);
        return {addOrder(modified_order), cancel_result};
    }
}

OrderResult OrderBook::cancelOrder(const info::CancelOrder& cancel_order) {
    auto itr = limitorders_.find(cancel_order.order_id);
    if (itr == limitorders_.end()) {
        return OrderResult(info::RejectionReason::order_not_found);
    }
    if (itr->second.order.getUserID() != cancel_order.user_id) {
        return OrderResult(info::RejectionReason::wrong_user_id);
    }
    Limit& limit = itr->second;
    if (limit.next_limit == nullptr) { // cancel tail order
        limit.current_level->tail = limit.prev_limit;
        if (limit.prev_limit != nullptr)
            limit.prev_limit->next_limit = nullptr;
    }
    if (limit.prev_limit == nullptr) { // cancel head order
        limit.current_level->head = limit.next_limit;
        if (limit.next_limit != nullptr)
            limit.next_limit->prev_limit = nullptr;
    }
    if (limit.next_limit != nullptr && limit.prev_limit != nullptr) { // cancel non-tail non-head order
        limit.next_limit->prev_limit = limit.prev_limit;
        limit.prev_limit->next_limit = limit.next_limit;
    }
    if (limit.current_level->getLevelOrderCount() == 0) {
        if (itr->second.order.isBuySide())
            bids_.erase(itr->second.order.getPrice());
        else
            asks_.erase(itr->second.order.getPrice());
    }

    limitorders_.erase(itr);
    OrderResult cancel_result;
    populateCancelOrderStatus(cancel_order, cancel_result);
    return cancel_result; // return cancel order ack
}

bool OrderBook::modifyOrderTrivial(const info::ModifyOrder& modify_order, const Order& order) {
    return modify_order.price == order.getPrice()
        && modify_order.quantity == order.getCurrQty();
}

template<typename Side>
Level& OrderBook::getSideLevel(const uint64_t price, Side& sidebook) {
    auto lvlitr = sidebook.find(price);
    if (lvlitr == sidebook.end()) {
        auto ret = sidebook.emplace(price, Level(price));
        if (!ret.second) {
            // error
        }
        lvlitr = ret.first;
    }
    return lvlitr->second;
}

bool OrderBook::orderFilled(const OptMatchResult& match_result) const {
    return match_result != std::nullopt && match_result->orderCompletelyFilled();
}

void OrderBook::populateNewOrderStatus(const tradeorder::Order& order, 
OrderResult& order_result) const {
    auto& order_status = order_result.orderstatus.new_order_status;
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
    auto& order_status = order_result.orderstatus.modify_order_status;
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
    auto& order_status = order_result.orderstatus.cancel_order_status;
    order_status.order_id = order.order_id;
    order_status.user_id = order.user_id;
    order_status.ticker = order.ticker;
    order_status.timestamp = util::getUnixTimestamp();
    order_result.order_status_present = info::OrderStatusPresent::CancelOrderPresent;
}
