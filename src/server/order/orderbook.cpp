#include "orderbook.hpp"
#include <iostream>

using namespace server::tradeorder;

inline void OrderBook::placeLimitInBookLevel(Level* level, Order& order) {
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
}

inline void OrderBook::placeOrderInBidBook(Order& order) {
    Level* level = &getSideLevel(order.getPrice(), bids_);
    level->is_buy_side = 1;
    placeLimitInBookLevel(level, order);
}

inline void OrderBook::placeOrderInAskBook(Order& order) {
    Level* level = &getSideLevel(order.getPrice(), asks_);
    level->is_buy_side = 0;
    placeLimitInBookLevel(level, order);
}

// these will be inlined and are just for readability

inline AddOrderResult OrderBook::fullyFilledResult(Order& order, OptMatchResult& match_result) {
    return AddOrderResult(match_result);
}

inline AddOrderResult OrderBook::partiallyFilledResult(Order& order, OptMatchResult& match_result) {
    return AddOrderResult(
        match_result,
        AddOrderStatus(
            util::getUnixTimestamp(),
            order.getPrice(),
            order.getCurrQty(),
            order.isBuySide(),
            order.getOrderID(),
            order.getTicker(),
            order.getUserID()
        )
    );
}

inline AddOrderResult OrderBook::noFillResult(Order& order) {
    return AddOrderResult(
        AddOrderStatus(
            util::getUnixTimestamp(),
            order.getPrice(),
            order.getCurrQty(),
            order.isBuySide(),
            order.getOrderID(),
            order.getTicker(),
            order.getUserID()
        )
    );
}

ModifyOrderResult OrderBook::modifyOrder(const info::ModifyOrder& modify_order) {
    using namespace info;
    auto itr = limitorders_.find(modify_order.order_id);
    if (itr == limitorders_.end()) {
        return ModifyOrderResult(ModifyRejectionReason::order_not_found);
    }
    auto& limit = itr->second;
    if (limit.order.isBuySide() != modify_order.is_buy_side) {
        return ModifyOrderResult(ModifyRejectionReason::modify_wrong_side);
    }
    if (itr->second.order.getUserID() != modify_order.user_id) {
        return ModifyOrderResult(ModifyRejectionReason::wrong_user_id);
    }
    if (modifyOrderTrivial(modify_order, limit.order)) {
        return ModifyOrderResult(ModifyRejectionReason::modification_trivial);
    }
    if (modify_order.price == limit.order.getPrice()) {
        OrderResult order_result;
        populateModifyOrderStatus(modify_order, order_result);
        if (limit.order.getCurrQty() < modify_order.quantity)
            limit.order.increaseQty(modify_order.quantity + limit.order.getCurrQty());
        else
            limit.order.decreaseQty(limit.order.getCurrQty() - modify_order.quantity);
        return ModifyOrderResult(
            AddOrderResult(
                AddOrderStatus(
                    util::getUnixTimestamp(),
                    modify_order.price,
                    modify_order.quantity,
                    modify_order.is_buy_side,
                    modify_order.order_id,
                    modify_order.ticker,
                    modify_order.user_id
                ) 
            )
        );
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
        CancelOrderResult cancel_result = cancelOrder(original_order);
        return ModifyOrderResult(addOrder<Side::Sell>(modified_order), cancel_result);
    }
}

CancelOrderResult OrderBook::cancelOrder(const info::CancelOrder& cancel_order) {
    auto itr = limitorders_.find(cancel_order.order_id);
    if (itr == limitorders_.end()) {
        return CancelOrderResult(info::CancelRejectionReason::order_not_found);
    }
    if (itr->second.order.getUserID() != cancel_order.user_id) {
        return CancelOrderResult(info::CancelRejectionReason::wrong_user_id);
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
    return CancelOrderResult(
        info::CancelOrderStatus(util::getUnixTimestamp(), cancel_order)
    );
}

bool OrderBook::modifyOrderTrivial(const info::ModifyOrder& modify_order, const Order& order) {
    return modify_order.price == order.getPrice()
        && modify_order.quantity == order.getCurrQty();
}

bool OrderBook::orderFilled(const OptMatchResult& match_result) const {
    return match_result != std::nullopt && match_result->orderCompletelyFilled();
}
