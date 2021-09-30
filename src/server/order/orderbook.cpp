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

OrderBook::OrderBook(rpc::MarketDataDispatcher* md_dispatch)
    : md_dispatch_(md_dispatch) 
{}

OrderBook::OrderBook(const OrderBook& orderbook) {
    md_dispatch_ = orderbook.getMDDispatcher();
}

void OrderBook::addOrder(Order& order) {
    std::unique_lock<std::mutex> lock(orderbook_mutex_, std::try_to_lock);
    if (limitorders_.find(order.getOrderID()) != limitorders_.end()) {
        #ifndef TEST_BUILD
        order.connection_->sendRejection(static_cast<Rejection>(ORDER_ID_ALREADY_PRESENT),
            order.getUserID(), order.getOrderID(), order.getTicker());
        #endif
        return;
    }
    switch(order.isBuySide()) {
        case true: 
            addBidOrder(order); 
            break;
        case false: 
            addAskOrder(order); 
            break;
    }
}

void OrderBook::addBidOrder(Order& order) {
    if (possibleMatches(asks_, order)) {
        matchOrder(order, asks_);
        return;
    }
    placeOrderInBook(order, bids_, order.isBuySide());
    sendOrderAddedToDispatcher(order);
}

void OrderBook::addAskOrder(Order& order) {
    if (possibleMatches(bids_, order)) {
        matchOrder(order, bids_);
        return;
    }
    placeOrderInBook(order, asks_, order.isBuySide());
    sendOrderAddedToDispatcher(order);
}

// these will be inlined (hopefully) and are just for readability
inline void OrderBook::placeLimitInBookLevel(Level& level, ::tradeorder::Order& order) {
    auto limitr = limitorders_.emplace(order.getOrderID(), Limit(order));
    if (!limitr.second) {
        logging::Logger::Log(
            logging::LogType::Error,
            util::getLogTimestamp(),
            "Failed to emplace order in limitbook", util::ShortString(order.getTicker()),
            "Order ID", order.getOrderID()
        );
        throw EngineException(
            "Unable to emplace new order in limitorder book, Book ID: "
            + std::to_string(ticker_) + " Order ID: " + std::to_string(order.getOrderID())
        );
    }
    Limit& limit = limitr.first->second;
    if (level.head == nullptr) {
        level.head = &limit;
        level.tail = &limit;
    }
    else {
        Limit* limit_temp = level.tail;
        level.tail = &limit;
        limit.prev_limit = limit_temp;
        limit_temp->next_limit = &limit;
    }
    limit.current_level = &level;
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

void OrderBook::modifyOrder(const ModifyOrder& modify_order) {
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
    error_flags |= (limit.order.getUserID() != modify_order.user_id) << 2;
    error_flags |= modifyOrderTrivial(modify_order, limit.order) << 3;
    if (error_flags) {
        processModifyError(error_flags, modify_order);
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
    Order new_order(modify_order);
    addOrder(new_order);
}

inline void OrderBook::processModifyError(uint8_t error_flags, const ModifyOrder& order) {
    bool wrong_side = (error_flags >> 1) & 1U;
    if (wrong_side)
        sendRejection(static_cast<Rejection>(MODIFY_WRONG_SIDE), order);
    bool wrong_userid = (error_flags >> 2) & 1U;
    if (wrong_userid)
        sendRejection(static_cast<Rejection>(WRONG_USER_ID), order);
    bool trivial_modify = (error_flags >> 3) & 1U;
    if (trivial_modify)
        sendRejection(static_cast<Rejection>(MODIFICATION_TRIVIAL), order);
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
        if (limit.order.isBuySide()) // if head and tail, its last order in level, so erase level
            bids_.erase(limit.order.getPrice());
        else
            asks_.erase(limit.order.getPrice());
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

void OrderBook::sendOrderAddedToDispatcher(const Order& order) {
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

bool OrderBook::modifyOrderTrivial(const info::ModifyOrder& modify_order, const Order& order) {
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
