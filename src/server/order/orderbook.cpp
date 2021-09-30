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
        logging::Logger::Log(
            logging::LogType::Info, 
            util::getLogTimestamp(), 
            "Order filled with ID:", fill.order_id, 
            "User ID:", util::ShortString(fill.user_id),
            "Fill quantity:", fill.fill_qty
        );
        *orderfill_data.mutable_fill() = std::move(orderfill_ack.fill());
        md_dispatch_->writeMarketData(&orderfill_data);
    }
    #endif
}

void OrderBook::modifyOrder(const info::ModifyOrder& modify_order) {
    std::unique_lock<std::mutex> lock(orderbook_mutex_, std::try_to_lock);
    using namespace info;
    auto itr = limitorders_.find(modify_order.order_id);
    if (itr == limitorders_.end()) {
        sendRejection(static_cast<Rejection>(ORDER_NOT_FOUND), modify_order);
        return;
    }
    auto& limit = itr->second;
    uint8_t error_flags = 0;
    error_flags |= (limit.order.isBuySide() != modify_order.is_buy_side) << 1;
    error_flags |= (itr->second.order.getUserID() != modify_order.user_id) << 2;
    error_flags |= modifyOrderTrivial(modify_order, limit.order) << 3;
    if (error_flags) {
        processError(error_flags, modify_order);
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
    cancelOrder(modify_order);
    ::tradeorder::Order new_order(modify_order);
    (this->*OrderBook::add_order[modify_order.is_buy_side])(new_order);
}

void OrderBook::cancelOrder(const info::CancelOrder& cancel_order) {
    std::unique_lock<std::mutex> lock(orderbook_mutex_, std::try_to_lock);
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
    if (isInMiddleOfLevel(limit)) {
        limit.next_limit->prev_limit = limit.prev_limit;
        limit.prev_limit->next_limit = limit.next_limit;
    }
    else if (isHeadOrder(limit)) {
        limit.current_level->head = limit.next_limit;
        limit.next_limit->prev_limit = nullptr;
    }
    else if (isTailOrder(limit)) {
        limit.current_level->tail = limit.prev_limit;
        limit.prev_limit->next_limit = nullptr;
    }
    else if (isHeadAndTail(limit)) {
        if (itr->second.order.isBuySide()) // if head and tail, its last order in level, so erase level
            bids_.erase(itr->second.order.getPrice());
        else
            asks_.erase(itr->second.order.getPrice());
    }
    limitorders_.erase(itr);
    sendOrderCancelledToDispatcher(cancel_order);
}

inline bool OrderBook::isTailOrder(const Limit& lim) const {
    return lim.next_limit == nullptr && lim.prev_limit != nullptr;
}

inline bool OrderBook::isHeadOrder(const Limit& lim) const {
    return lim.prev_limit == nullptr && lim.next_limit != nullptr;
}

inline bool OrderBook::isHeadAndTail(const Limit& lim) const {
    return lim.next_limit == nullptr && lim.prev_limit == nullptr;
}

inline bool OrderBook::isInMiddleOfLevel(const Limit& lim) const {
    return lim.next_limit != nullptr && lim.prev_limit != nullptr;
}

void OrderBook::sendOrderAddedToDispatcher(const ::tradeorder::Order& order) {
    logging::Logger::Log(
        logging::LogType::Info, 
        util::getLogTimestamp(), 
        "Order added with ID:", order.getOrderID(), 
        "User ID:", util::ShortString(order.getUserID()), 
        "Ticker:", util::ShortString(order.getTicker()),
        "Price:", order.getPrice(), 
        "Quantity:", order.getCurrQty()
    );
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
    logging::Logger::Log(
        logging::LogType::Info, 
        util::getLogTimestamp(), 
        "Order cancelled:", cancel_order.order_id, 
        "User ID:", util::ShortString(cancel_order.user_id), 
        "Ticker:", util::ShortString(cancel_order.ticker)
    );
    #ifndef TEST_BUILD
    auto cancel_data = OrderBook::cancelorder_data.mutable_cancel();
    cancel_data->set_order_id(cancel_order.order_id);
    cancel_data->set_timestamp(util::getUnixTimestamp());
    md_dispatch_->writeMarketData(&OrderBook::cancelorder_data);
    #endif
}

void OrderBook::sendOrderModifiedToDispatcher(const info::ModifyOrder& modify_order) {
    logging::Logger::Log(
        logging::LogType::Info, 
        util::getLogTimestamp(), 
        "Order modified:", modify_order.order_id, 
        "User ID:", util::ShortString(modify_order.user_id), 
        "Ticker:", util::ShortString(modify_order.ticker), 
        "To Quantity:", modify_order.quantity, 
        "Side:", modify_order.is_buy_side
    );
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
        logging::Logger::Log(
            logging::LogType::Warning, 
            util::getLogTimestamp(), 
            "Failed to find order with ID:", order_id
        );
        ::tradeorder::Order dangler;
        return {false, dangler};
    }
    logging::Logger::Log(
        logging::LogType::Debug, 
        util::getLogTimestamp(), 
        "Successfully found order with ID:", order_id
    );
    return {true, itr->second.order};
}
