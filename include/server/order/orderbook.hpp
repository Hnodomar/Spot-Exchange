#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstdint>
#include <map>

#include "order.hpp"
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
using MatchResult = server::matching::MatchResult;
using OrderResult = info::OrderResult;
using BidMatcher = MatchResult (*)(Order&, bidbook&, limitbook&);
using AskMatcher = MatchResult (*)(Order&, askbook&, limitbook&);
class OrderBook {
public:
    OrderBook(
        BidMatcher bidmatcher = &server::matching::FIFOMatch<bidbook>, 
        AskMatcher askmatcher = &server::matching::FIFOMatch<askbook>
    ): MatchBids(bidmatcher), MatchAsks(askmatcher) {}
    OrderResult addOrder(::tradeorder::Order&& order);
    OrderResult modifyOrder(info::ModifyOrder& modify_order);
    OrderResult cancelOrder(info::CancelOrder& cancel_order);
private:
    template<typename T>
    Level& getSideLevel(const uint64_t price, T sidebook);
    uint64_t ticker_;
    askbook asks_;
    bidbook bids_;
    limitbook limitorders_;
    MatchResult (*MatchBids)(Order& order_to_match, bidbook& bids, limitbook& limitbook);
    MatchResult (*MatchAsks)(Order& order_to_match, askbook& asks, limitbook& limitbook);
};
}
}
#endif
