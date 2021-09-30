#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstdint>
#include <map>
#include <array>
#include <iostream>
#include <utility>
#include <mutex>
#ifndef TEST_BUILD
#include "orderentrystreamconnection.hpp"
#include "marketdatadispatcher.hpp"
#else 
namespace rpc {class MarketDataDispatcher;}
#endif
#include "logger.hpp"
#include "exception.hpp"
#include "fifomatching.hpp"
#include "order.hpp"
#include "limit.hpp"

static constexpr uint8_t UNKNOWN = 0;
static constexpr uint8_t ORDER_NOT_FOUND = 1;
static constexpr uint8_t ORDER_ID_ALREADY_PRESENT = 2;
static constexpr uint8_t ORDERBOOK_NOT_FOUND = 3;
static constexpr uint8_t TICKER_NOT_FOUND = 4;
static constexpr uint8_t MODIFY_WRONG_SIDE = 5;
static constexpr uint8_t MODIFICATION_TRIVIAL = 6;
static constexpr uint8_t WRONG_USER_ID = 7;

namespace server {
namespace tradeorder {
using namespace info;
using price = uint64_t;
using order_id = uint64_t;
using askbook = std::map<price, Level>;
using bidbook = std::map<price, Level, std::greater<price>>;
using limitbook = std::unordered_map<order_id, server::tradeorder::Limit>;
using MatchResult = server::matching::MatchResult;
using BidMatcher = MatchResult (*)(::tradeorder::Order&, bidbook&, limitbook&);
using AskMatcher = MatchResult (*)(::tradeorder::Order&, askbook&, limitbook&);  
using GetOrderResult = std::pair<bool, ::tradeorder::Order&>;
#ifndef TEST_BUILD
using Rejection = orderentry::OrderEntryRejection::RejectionReason; 
using MDResponse = orderentry::MarketDataResponse;
#else
enum Rejection {
    UNKNOWN = 1,
    ORDER_NOT_FOUND = 1,
    ORDER_ID_ALREADY_PRESENT = 2,
    ORDERBOOK_NOT_FOUND = 3,
    TICKER_NOT_FOUND = 4,
    MODIFY_WRONG_SIDE = 5,
    MODIFICATION_TRIVIAL = 6,
    WRONG_USER_ID = 7,
};
#endif
enum class Side {Sell, Buy};

class OrderBook {
public:
    using AddOrderFn = void (server::tradeorder::OrderBook::*)(::tradeorder::Order&);
    OrderBook(
        rpc::MarketDataDispatcher* md_dispatch, 
        BidMatcher bidmatcher = &server::matching::FIFOMatch<bidbook>, 
        AskMatcher askmatcher = &server::matching::FIFOMatch<askbook>
    );
    OrderBook(const OrderBook& orderbook);
    OrderBook();
    void addOrder(::tradeorder::Order& order);
    void modifyOrder(const info::ModifyOrder& modify_order);
    void cancelOrder(const info::CancelOrder& cancel_order);
    GetOrderResult getOrder(uint64_t order_id);
    uint64_t numOrders() const {return limitorders_.size();}
    uint64_t numLevels() const {return asks_.size() + bids_.size();}
    BidMatcher getBidMatcher() const {return MatchBids;}
    AskMatcher getAskMatcher() const {return MatchAsks;}
    rpc::MarketDataDispatcher* getMDDispatcher() const {return md_dispatch_;}
private:
    template<Side T> void addOrder(::tradeorder::Order& order);
    static const std::array<AddOrderFn, 2> add_order;
    template<typename T> Level& getSideLevel(const uint64_t price, T& sidebook);
    template<typename OrderType> void sendRejection(Rejection rejection, const OrderType& order);
    template<typename OrderType> void processError(uint8_t error_flags, const OrderType& order);
    void placeOrderInBidBook(::tradeorder::Order& order);
    void placeOrderInAskBook(::tradeorder::Order& order);
    void communicateMatchResults(MatchResult& match_result, const ::tradeorder::Order& order) const;
    bool possibleMatches(const askbook& book, const ::tradeorder::Order& order) const;
    bool possibleMatches(const bidbook& book, const ::tradeorder::Order& order) const;
    bool modifyOrderTrivial(const info::ModifyOrder& modify_order, const ::tradeorder::Order& order);
    void placeLimitInBookLevel(Level* level, ::tradeorder::Order& order);
    void sendOrderAddedToDispatcher(const ::tradeorder::Order& order);
    void sendOrderCancelledToDispatcher(const info::CancelOrder& cancel_order);
    void sendOrderModifiedToDispatcher(const info::ModifyOrder& modify_order);
    bool isTailOrder(const Limit& lim) const;
    bool isHeadOrder(const Limit& lim) const;
    bool isHeadAndTail(const Limit& lim) const;
    bool isInMiddleOfLevel(const Limit& lim) const;
    uint64_t ticker_;
    askbook asks_;
    bidbook bids_;
    std::unordered_map<order_id, Limit> limitorders_;
    std::mutex orderbook_mutex_;
    MatchResult (*MatchBids)(::tradeorder::Order& order_to_match, bidbook& bids, limitbook& limitbook);
    MatchResult (*MatchAsks)(::tradeorder::Order& order_to_match, askbook& asks, limitbook& limitbook);
    rpc::MarketDataDispatcher* md_dispatch_;
    #ifndef TEST_BUILD
    static thread_local orderentry::OrderEntryResponse orderfill_ack;
    static thread_local MDResponse orderfill_data;
    static thread_local MDResponse neworder_data;
    static thread_local MDResponse modorder_data;
    static thread_local MDResponse cancelorder_data;
    #endif
};

template<typename OrderType>
inline void OrderBook::processError(uint8_t error_flags, const OrderType& order) {
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

template<typename OrderType>
inline void OrderBook::sendRejection(Rejection rejection, const OrderType& order) {
    #ifndef TEST_BUILD
    order.connection->sendRejection(rejection, order.user_id, order.order_id, order.ticker);
    #endif
}

template<typename Side>
inline Level& OrderBook::getSideLevel(const uint64_t price, Side& sidebook) {
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

template<>
inline void OrderBook::addOrder<Side::Buy>(::tradeorder::Order& order) {
    if (limitorders_.find(order.getOrderID()) != limitorders_.end()) {
        #ifndef TEST_BUILD
        order.connection_->sendRejection(static_cast<Rejection>(ORDER_ID_ALREADY_PRESENT),
            order.getUserID(), order.getOrderID(), order.getTicker());
        #endif
        return;
    }
    if (possibleMatches(asks_, order)) {
        auto match_result = MatchAsks(order, asks_, limitorders_);
        if (order.getCurrQty() != 0) {
            placeOrderInBidBook(order); // want to update book before anyone is informed of result
        }
        communicateMatchResults(match_result, order);
        return;
    }
    placeOrderInBidBook(order);
    sendOrderAddedToDispatcher(order);
}

template<>
inline void OrderBook::addOrder<Side::Sell>(::tradeorder::Order& order) {
    if (limitorders_.find(order.getOrderID()) != limitorders_.end()) {
        #ifndef TEST_BUILD
        order.connection_->sendRejection(static_cast<Rejection>(ORDER_ID_ALREADY_PRESENT),
            order.getUserID(), order.getOrderID(), order.getTicker());
        #endif
        return;
    }
    if (possibleMatches(bids_, order)) {
        auto match_result = MatchBids(order, bids_, limitorders_);
        if (order.getCurrQty() != 0) {
            placeOrderInAskBook(order); // want to update book before anyone is informed of result
        }
        communicateMatchResults(match_result, order);
        return;
    }
    placeOrderInAskBook(order);
    sendOrderAddedToDispatcher(order);
}

}
}

#endif
