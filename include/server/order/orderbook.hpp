#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstdint>
#include <map>

#include "fifomatching.hpp"
#include "orderresult.hpp"
#include "exception.hpp"

namespace server {
namespace tradeorder {
using price = uint64_t;
using order_id = uint64_t;
using Order = ::tradeorder::Order;
using askbook = std::map<price, Level>;
using bidbook = std::map<price, Level, std::greater<price>>;
using limitbook = std::unordered_map<order_id, Limit>;
using OptMatchResult = std::optional<server::matching::MatchResult>;
using OrderResult = info::OrderResult;
using BidMatcher = OptMatchResult (*)(Order&, bidbook&, limitbook&);
using AskMatcher = OptMatchResult (*)(Order&, askbook&, limitbook&);
class OrderBook {
public:
    OrderBook(
        BidMatcher bidmatcher = &server::matching::FIFOMatch<bidbook>, 
        AskMatcher askmatcher = &server::matching::FIFOMatch<askbook>
    ): MatchBids(bidmatcher), MatchAsks(askmatcher) {}
    OrderResult addOrder(Order& order);
    std::pair<OrderResult, OrderResult> modifyOrder(const info::ModifyOrder& modify_order);
    OrderResult cancelOrder(const info::CancelOrder& cancel_order);
    uint64_t numOrders() const {return limitorders_.size();}
    uint64_t numLevels() const {return asks_.size() + bids_.size();}
private:
    template<typename T>
    Level& getSideLevel(const uint64_t price, T& sidebook);
    bool orderFilled(const OptMatchResult& match_result) const;
    bool modifyOrderTrivial(const info::ModifyOrder& modify_order, const Order& order);
    void populateNewOrderStatus(
        const tradeorder::Order& order,
        OrderResult& order_result
    ) const;
    void populateModifyOrderStatus(
        const info::ModifyOrder& order,
        OrderResult& order_result
    ) const;
    void populateCancelOrderStatus(
        const info::CancelOrder& order,
        OrderResult& order_result
    ) const;
    uint64_t ticker_;
    askbook asks_;
    bidbook bids_;
    limitbook limitorders_;
    OptMatchResult (*MatchBids)(Order& order_to_match, bidbook& bids, limitbook& limitbook);
    OptMatchResult (*MatchAsks)(Order& order_to_match, askbook& asks, limitbook& limitbook);
};
}
}
#endif
