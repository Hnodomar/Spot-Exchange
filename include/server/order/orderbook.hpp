#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstdint>
#include <map>

#include "order.hpp"
#include "fifomatching.hpp"
#include "exception.hpp"

namespace server {
namespace tradeorder {
using price = uint64_t;
using order_id = uint64_t;
using Order = ::tradeorder::Order;
using askbook = std::map<price, Level>;
using bidbook = std::map<price, Level, std::greater<price>>;
using MatchResult = server::matching::MatchResult;
using BidMatcher = MatchResult (*)(Order&, bidbook&);
using AskMatcher = MatchResult (*)(Order&, askbook&);
class OrderBook {
public:
    OrderBook(
        BidMatcher bidmatcher = &server::matching::FIFOMatch<bidbook>, 
        AskMatcher askmatcher = &server::matching::FIFOMatch<askbook>
    ): MatchBids(bidmatcher), MatchAsks(askmatcher) {}
    MatchResult addOrder(::tradeorder::Order&& order) {
        Level* level = nullptr;
        MatchResult match_result;
        if (order.getSide() == 'B') {
            match_result = MatchBids(order, bids_);
            if (processMatchResults(match_result))
                return match_result;
            level = &getSideLevel(order.getPrice(), bids_);
        }
        else {
            match_result = MatchAsks(order, asks_);
            if (processMatchResults(match_result))
                return match_result;
            level = &getSideLevel(order.getPrice(), asks_);
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
        return match_result;
    }
    void modifyOrder(::info::ModifyOrder* modify_order) {
        auto itr = limitorders_.find(modify_order->order_id);
        if (itr == limitorders_.end())
            return; //error
        auto& limit = itr->second;
        limit.order.decreaseQty(modify_order->quantity);
    }
private:
    template<typename T>
    Level& getSideLevel(const uint64_t price, T sidebook) {
        auto lvlitr = sidebook.find(price);
        if (lvlitr == sidebook.end()) {
            auto ret = sidebook.emplace(price, Level());
            if (!ret.second) {
                // error
            }
            lvlitr = ret.first;
        }
        return lvlitr->second;
    }
    bool processMatchResults(MatchResult match_result) {
        for (const auto& fill : match_result.getFills()) {
            if (fill.full_fill) {
                limitorders_.erase(fill.order_id);
            }
        }
        return match_result.orderCompletelyFilled();
    }
    uint64_t ticker_;
    askbook asks_;
    bidbook bids_;
    std::unordered_map<order_id, Limit> limitorders_;
    MatchResult (*MatchBids)(Order& order_to_match, bidbook& bids);
    MatchResult (*MatchAsks)(Order& order_to_match, askbook& bids);
};
}
}
#endif
