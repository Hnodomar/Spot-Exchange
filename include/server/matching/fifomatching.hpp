#ifndef FIFO_MATCHING_HPP
#define FIFO_MATCHING_HPP

#include <map>
#include <functional>

#include "matchresult.hpp"
#include "level.hpp"
#include "util.hpp"

namespace server {
namespace matching {
using price = uint64_t;
using Level = server::tradeorder::Level;
using Order = ::tradeorder::Order;
using Limit = server::tradeorder::Limit;
using askbook = std::map<price, Level>;
using bidbook = std::map<price, Level, std::greater<price>>;

static bool comparePrices(bidbook&, uint64_t order_price, uint64_t bid_price) {
    return order_price > bid_price;
}

static bool comparePrices(askbook&, uint64_t order_price, uint64_t ask_price) {
    return order_price < ask_price;
}

template <typename T>
MatchResult FIFOMatch(Order& order_to_match, T& book) {
    MatchResult match_result;
    if (book.empty())
        return match_result;
    auto book_itr = book.begin();
    Level& book_lvl = book_itr->second;
    uint64_t closest_book_price = book_itr->first;
    uint64_t order_price = order_to_match.getPrice();
    while (book_itr != book.end()) {
        if (comparePrices(book, order_price, closest_book_price)) // no matching to do
            return match_result;
        Limit* book_lim = book_lvl.head;
        while (book_lim != nullptr) {
            uint16_t book_qty = book_lim->order.getCurrQty();
            uint16_t fill_qty = std::min(book_qty, order_to_match.getCurrQty());
            order_to_match.decreaseQty(fill_qty);
            book_lim->order.decreaseQty(fill_qty);
            int64_t filltime = ::util::getUnixTimestamp();
            match_result.addFill(
                filltime,
                order_to_match.getTicker(),
                book_lim->order.getOrderID(), 
                book_lim->order.getPrice(),
                fill_qty,
                book_lim->order.getCurrQty() == 0,
                book_lim->order.getUserID()
            );
            match_result.addFill(
                filltime,
                order_to_match.getTicker(),
                order_to_match.getOrderID(),
                book_lim->order.getPrice(),
                fill_qty,
                order_to_match.getCurrQty() == 0,
                book_lim->order.getUserID()
            );
            if (order_to_match.getCurrQty() == 0) { //order doesnt get added
                match_result.setOrderFilled();
                return match_result;
            }
            Limit* temp_lim = book_lim->next_limit;
            book_lvl.head = temp_lim;
            temp_lim->prev_limit = nullptr;
            book_lim = book_lim->next_limit;
        }
        ++book_itr;
        book_lvl = book_itr->second;
    }
    return match_result;
}

template MatchResult FIFOMatch<bidbook>(Order&, bidbook&);
template MatchResult FIFOMatch<askbook>(Order&, askbook&);

}
}

#endif
