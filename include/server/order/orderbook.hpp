#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstdint>
#include <map>
#include <array>

#include "fifomatching.hpp"
#include "orderresult.hpp"
#include "exception.hpp"

namespace server {
namespace tradeorder {
using namespace info;
using price = uint64_t;
using order_id = uint64_t;
using Order = ::tradeorder::Order;
using askbook = std::map<price, Level>;
using bidbook = std::map<price, Level, std::greater<price>>;
using limitbook = std::unordered_map<order_id, Limit>;
using OptMatchResult = std::optional<server::matching::MatchResult>;
using OrderResult = OrderResult;
using BidMatcher = OptMatchResult (*)(Order&, bidbook&, limitbook&);
using AskMatcher = OptMatchResult (*)(Order&, askbook&, limitbook&);
using AddOrderFn = AddOrderResult (server::tradeorder::OrderBook::*)(Order&);
    
enum class Side {Sell, Buy};

class OrderBook {
public:
    OrderBook(
      BidMatcher bidmatcher = &server::matching::FIFOMatch<bidbook>, 
      AskMatcher askmatcher = &server::matching::FIFOMatch<askbook>
    )
     : MatchBids(bidmatcher)
     , MatchAsks(askmatcher)
     , add_order{&addOrder<Side::Sell>, &addOrder<Side::Buy>}
    {}
    const std::array<AddOrderFn, 2> add_order;
    ModifyOrderResult modifyOrder(const info::ModifyOrder& modify_order);
    CancelOrderResult cancelOrder(const info::CancelOrder& cancel_order);
    uint64_t numOrders() const {return limitorders_.size();}
    uint64_t numLevels() const {return asks_.size() + bids_.size();}
private:
    template<Side T>
    AddOrderResult addOrder(Order& order);
    template<typename T>
    Level& getSideLevel(const uint64_t price, T& sidebook);
    void placeOrderInBidBook(Order& order);
    void placeOrderInAskBook(Order& order);
    AddOrderResult fullyFilledResult(Order& order, OptMatchResult& match_result);
    AddOrderResult partiallyFilledResult(Order& order, OptMatchResult& match_result);
    AddOrderResult noFillResult(Order& order);
    bool orderFilled(const OptMatchResult& match_result) const;
    bool modifyOrderTrivial(const info::ModifyOrder& modify_order, const Order& order);
    void placeLimitInBookLevel(Level* level, Order& order);
    void populateModifyOrderStatus(
        const info::ModifyOrder& order,
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
inline AddOrderResult OrderBook::addOrder<Side::Buy>(Order& order) {
    if (limitorders_.find(order.getOrderID()) != limitorders_.end()) {
        return AddOrderResult(info::AddOrderRejectionReason::order_id_already_present);
    }
    if (asks_.begin()->first <= order.getPrice()) {
        auto match_result = MatchAsks(order, asks_, limitorders_);
        if (orderFilled(match_result))
            return fullyFilledResult(order, match_result);
        placeOrderInBidBook(order);
        return partiallyFilledResult(order, match_result);
    }
    placeOrderInBidBook(order);
    return noFillResult(order);
}

template<>
inline AddOrderResult OrderBook::addOrder<Side::Sell>(Order& order) {
    if (limitorders_.find(order.getOrderID()) != limitorders_.end()) {
        return AddOrderResult(info::AddOrderRejectionReason::order_id_already_present);
    }
    if (bids_.begin()->first <= order.getPrice()) {
        auto match_result = MatchBids(order, bids_, limitorders_);
        if (orderFilled(match_result))
            return fullyFilledResult(order, match_result);
        placeOrderInAskBook(order);
        return partiallyFilledResult(order, match_result);
    }
    placeOrderInAskBook(order);
    return noFillResult(order);
}
