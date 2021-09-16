#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstdint>
#include <map>
#include <array>
#include <utility>

#include "orderentrystreamconnection.hpp"
#include "fifomatching.hpp"
#include "ordertypes.hpp"
#include "level.hpp"
#include "exception.hpp"

static constexpr uint8_t UNKNOWN = 0;
static constexpr uint8_t ORDER_NOT_FOUND = 1;
static constexpr uint8_t ORDER_ID_ALREADY_PRESENT = 2;
static constexpr uint8_t ORDERBOOK_NOT_FOUND = 3;
static constexpr uint8_t TICKER_NOT_FOUND = 4;
static constexpr uint8_t MODIFY_WRONG_SIDE = 5;
static constexpr uint8_t MODIFICATION_TRIVIAL = 6;
static constexpr uint8_t WRONG_USER_ID = 7;

using OEResponseType = orderentry::OrderEntryResponse;
thread_local OEResponseType orderfill_ack;

namespace server {
namespace tradeorder {
using namespace info;
using price = uint64_t;
using order_id = uint64_t;
using askbook = std::map<price, Level>;
using bidbook = std::map<price, Level, std::greater<price>>;
using limitbook = std::unordered_map<order_id, server::tradeorder::Limit>;
using Rejection = orderentry::OrderEntryRejection::RejectionReason;
using MatchResult = server::matching::MatchResult;
using BidMatcher = MatchResult (*)(::tradeorder::Order&, bidbook&, limitbook&);
using AskMatcher = MatchResult (*)(::tradeorder::Order&, askbook&, limitbook&);    
using Rejection = orderentry::OrderEntryRejection::RejectionReason;
enum class Side {Sell, Buy};

class OrderBook {
public:
    using AddOrderFn = void (server::tradeorder::OrderBook::*)(::tradeorder::Order&);
    OrderBook(
      BidMatcher bidmatcher = &server::matching::FIFOMatch<bidbook>, 
      AskMatcher askmatcher = &server::matching::FIFOMatch<askbook>
    )
     : MatchBids(bidmatcher)
     , MatchAsks(askmatcher)
    {}
    template<Side T> void addOrder(::tradeorder::Order& order);
    static const std::array<AddOrderFn, 2> add_order;
    void modifyOrder(const info::ModifyOrder& modify_order);
    void cancelOrder(const info::CancelOrder& cancel_order);
    uint64_t numOrders() const {return limitorders_.size();}
    uint64_t numLevels() const {return asks_.size() + bids_.size();}
private:
    template<typename T> Level& getSideLevel(const uint64_t price, T& sidebook);
    void placeOrderInBidBook(::tradeorder::Order& order);
    void placeOrderInAskBook(::tradeorder::Order& order);
    void processMatchResults(MatchResult& match_result, const ::tradeorder::Order& order) const;
    bool possibleMatches(const askbook& book, const ::tradeorder::Order& order) const;
    bool possibleMatches(const bidbook& book, const ::tradeorder::Order& order) const;
    bool modifyOrderTrivial(const info::ModifyOrder& modify_order, const ::tradeorder::Order& order);
    void placeLimitInBookLevel(Level* level, ::tradeorder::Order& order);
    uint64_t ticker_;
    askbook asks_;
    bidbook bids_;
    std::unordered_map<uint64_t, Limit> limitorders_;
    MatchResult (*MatchBids)(::tradeorder::Order& order_to_match, bidbook& bids, limitbook& limitbook);
    MatchResult (*MatchAsks)(::tradeorder::Order& order_to_match, askbook& asks, limitbook& limitbook);
};

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
        order.getConnection()->sendRejection(
            static_cast<Rejection>(ORDER_ID_ALREADY_PRESENT),
            order.getUserID(),
            order.getOrderID(),
            order.getTicker()
        );
    }
    if (possibleMatches(asks_, order)) {
        auto match_result = MatchAsks(order, asks_, limitorders_);
        placeOrderInBidBook(order); // want to update book before anyone is informed of result
        processMatchResults(match_result, order);
        return;
    }
    placeOrderInBidBook(order);
}

template<>
inline void OrderBook::addOrder<Side::Sell>(::tradeorder::Order& order) {
    if (limitorders_.find(order.getOrderID()) != limitorders_.end()) {
        order.getConnection()->sendRejection(
            static_cast<Rejection>(ORDER_ID_ALREADY_PRESENT),
            order.getUserID(),
            order.getOrderID(),
            order.getTicker()
        );
    }
    if (possibleMatches(bids_, order)) {
        auto match_result = MatchBids(order, bids_, limitorders_);
        placeOrderInAskBook(order); // want to update book before anyone is informed of result
        processMatchResults(match_result, order);
        return;
    }
    placeOrderInAskBook(order);
}

static const std::array<OrderBook::AddOrderFn, 2> add_order{
    &OrderBook::addOrder<Side::Sell>,
    &OrderBook::addOrder<Side::Buy>
};

}
}

#endif
