#include "orderbook.hpp"
#include <iostream>
#include <utility>

using namespace server::tradeorder;

#ifndef TEST_BUILD
thread_local orderentry::OrderEntryResponse OrderBook::orderfill_ack;
thread_local orderentry::MarketDataResponse OrderBook::orderfill_data;
thread_local orderentry::MarketDataResponse OrderBook::neworder_data;
thread_local orderentry::MarketDataResponse OrderBook::modorder_data;
thread_local orderentry::MarketDataResponse OrderBook::cancelorder_data;
#endif

OrderBook::OrderBook(rpc::MarketDataDispatcher* md_dispatch, BidMatcher bidmatcher, AskMatcher askmatcher)
    : MatchBids(bidmatcher), MatchAsks(askmatcher), md_dispatch_(md_dispatch) 
{}

OrderBook::OrderBook(const OrderBook& orderbook) {
    MatchBids = orderbook.getBidMatcher();
    MatchAsks = orderbook.getAskMatcher();
    md_dispatch_ = orderbook.getMDDispatcher();
}

OrderBook::OrderBook() {
    MatchBids = &server::matching::FIFOMatch<bidbook>;
    MatchAsks = &server::matching::FIFOMatch<askbook>;
    md_dispatch_ = nullptr;
}

const std::array<OrderBook::AddOrderFn, 2> OrderBook::add_order{
    &OrderBook::addOrder<Side::Sell>,
    &OrderBook::addOrder<Side::Buy>
};

void OrderBook::addOrder(::tradeorder::Order& order) {
    std::lock_guard<std::mutex> lock(orderbook_mutex_);
    (this->*OrderBook::add_order[order.isBuySide()])(order);
}

// these will be inlined (hopefully) and are just for readability
inline void OrderBook::placeLimitInBookLevel(Level* level, ::tradeorder::Order& order) {
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

inline void OrderBook::placeOrderInBidBook(::tradeorder::Order& order) {
    Level* level = &getSideLevel(order.getPrice(), bids_);
    level->is_buy_side = 1;
    placeLimitInBookLevel(level, order);
}

inline void OrderBook::placeOrderInAskBook(::tradeorder::Order& order) {
    Level* level = &getSideLevel(order.getPrice(), asks_);
    level->is_buy_side = 0;
    placeLimitInBookLevel(level, order);
}

inline bool OrderBook::possibleMatches(const askbook& book, const ::tradeorder::Order& order) const {
    return book.begin()->first <= order.getPrice() && !book.empty();
}

inline bool OrderBook::possibleMatches(const bidbook& book, const ::tradeorder::Order& order) const {
    return book.begin()->first >= order.getPrice() && !book.empty();
}

void OrderBook::communicateMatchResults(MatchResult& match_result, const ::tradeorder::Order& order) const {
    #ifndef TEST_BUILD
    std::cout << "Communicating Match Results.." << std::endl;
    for (const auto& fill : match_result.getFills()) {
        auto fill_ack = orderfill_ack.mutable_fill();
        fill_ack->set_timestamp(fill.timestamp);
        fill_ack->set_fill_quantity(fill.fill_qty);
        fill_ack->set_complete_fill(fill.full_fill);
        auto common = fill_ack->mutable_status_common();
        common->set_order_id(order.getOrderID());
        common->set_ticker(order.getTicker());
        common->set_user_id(order.getUserID());
        const_cast<OrderEntryStreamConnection*>(
            fill.connection
        )->writeToClient(&orderfill_ack);
        *orderfill_data.mutable_fill() = std::move(orderfill_ack.fill());
        md_dispatch_->writeMarketData(&orderfill_data);
    }
    std::cout << "Finished Communicating Match Results" << std::endl;
    #endif
}

void OrderBook::modifyOrder(const info::ModifyOrder& modify_order) {
    using namespace info;
    auto itr = limitorders_.find(modify_order.order_id);
    if (itr == limitorders_.end()) {
        sendRejection(static_cast<Rejection>(ORDER_NOT_FOUND), modify_order);
        return;
    }
    auto& limit = itr->second;
    if (limit.order.isBuySide() != modify_order.is_buy_side) {
        sendRejection(static_cast<Rejection>(MODIFY_WRONG_SIDE), modify_order);
        return;
    }
    if (itr->second.order.getUserID() != modify_order.user_id) {
        sendRejection(static_cast<Rejection>(WRONG_USER_ID), modify_order);
        return;
    }
    if (modifyOrderTrivial(modify_order, limit.order)) {
        sendRejection(static_cast<Rejection>(MODIFICATION_TRIVIAL), modify_order);
        return;
    }
    if (modify_order.price == limit.order.getPrice()) {
        if (limit.order.getCurrQty() < modify_order.quantity)
            limit.order.increaseQty(modify_order.quantity + limit.order.getCurrQty());
        else
            limit.order.decreaseQty(limit.order.getCurrQty() - modify_order.quantity);
        sendOrderModifiedToDispatcher(modify_order);
        return;
    }
    else {
        cancelOrder(modify_order);
        ::tradeorder::Order new_order(modify_order);
        (this->*OrderBook::add_order[modify_order.is_buy_side])(new_order);
    }
}

void OrderBook::cancelOrder(const info::CancelOrder& cancel_order) {
    auto itr = limitorders_.find(cancel_order.order_id);
    if (itr == limitorders_.end()) {
        sendRejection(static_cast<Rejection>(ORDER_NOT_FOUND), cancel_order);
        return;
    }
    if (itr->second.order.getUserID() != cancel_order.user_id) {
        sendRejection(static_cast<Rejection>(WRONG_USER_ID), cancel_order);
        return;
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
    sendOrderCancelledToDispatcher(cancel_order);
}

void OrderBook::sendOrderAddedToDispatcher(const ::tradeorder::Order& order) {
    #ifndef TEST_BUILD
    auto add_data = OrderBook::neworder_data.mutable_add();
    add_data->set_order_id(order.getOrderID());
    add_data->set_ticker(order.getTicker());
    add_data->set_price(order.getPrice());
    add_data->set_quantity(order.getCurrQty());
    add_data->set_is_buy_side(order.isBuySide());
    add_data->set_timestamp(util::getUnixTimestamp());
    md_dispatch_->writeMarketData(&OrderBook::neworder_data);
    #endif
}

void OrderBook::sendOrderCancelledToDispatcher(const info::CancelOrder& cancel_order) {
    #ifndef TEST_BUILD
    auto cancel_data = OrderBook::cancelorder_data.mutable_cancel();
    cancel_data->set_order_id(cancel_order.order_id);
    cancel_data->set_timestamp(util::getUnixTimestamp());
    md_dispatch_->writeMarketData(&OrderBook::cancelorder_data);
    #endif
}

void OrderBook::sendOrderModifiedToDispatcher(const info::ModifyOrder& modify_order) {
    #ifndef TEST_BUILD
    auto modify_data = OrderBook::modorder_data.mutable_mod();
    modify_data->set_order_id(modify_order.order_id);
    modify_data->set_quantity(modify_order.quantity);
    md_dispatch_->writeMarketData(&OrderBook::modorder_data);
    #endif
}

bool OrderBook::modifyOrderTrivial(const info::ModifyOrder& modify_order, const ::tradeorder::Order& order) {
    return modify_order.price == order.getPrice() && modify_order.quantity == order.getCurrQty();
}

GetOrderResult OrderBook::getOrder(uint64_t order_id) {
    auto itr = limitorders_.find(order_id);
    if (itr == limitorders_.end()) {
        ::tradeorder::Order dangler;
        return {false, dangler};
    }
    return {true, itr->second.order};
}
