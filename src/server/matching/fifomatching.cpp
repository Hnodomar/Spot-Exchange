#include "fifomatching.hpp"
#include <iostream>

namespace server {
namespace matching { 
template <typename T>
std::optional<MatchResult> FIFOMatch(Order& order_to_match, T& book, limitbook& limitbook) {
    if (book.empty())  {
        return std::nullopt;
    }
    auto book_itr = book.begin();
    Level& book_lvl = book_itr->second;
    Limit* book_lim = book_lvl.head;
    uint64_t order_price = order_to_match.getPrice();
    MatchResult match_result;
    while (book_itr != book.end()) {
        if (noMatchingLevel(book, order_price, book_itr->first)) // no matching to do
            return match_result;
        book_lim = book_lvl.head;
        if (orderFullyMatchedInLevel(book_lim, order_to_match, match_result, 
          book_lvl, limitbook, book, book_itr)) {
            return match_result;
        }
        if (book_lvl.getLevelOrderCount() == 0) {
            book.erase(book_itr++);
        }
        else ++book_itr;
        book_lvl = book_itr->second;
    }
    return match_result;
}

inline void popLimitFromQueue(Limit*& book_lim, Level& book_lvl, limitbook& limitbook) {
    Limit* next_limit = book_lim->next_limit;
    if (next_limit != nullptr) {
        next_limit->prev_limit = nullptr;
    }
    book_lvl.head = next_limit;
    uint64_t book_lim_id = book_lim->order.getOrderID();
    book_lim = next_limit;
    limitbook.erase(book_lim_id);
}

inline void addFills(MatchResult& match_result, Order& order, Limit* book_lim, uint32_t fill_qty) {
    int64_t filltime = util::getUnixTimestamp();
    bool order_filled = order.getCurrQty() == 0;
    bool book_lim_filled = book_lim->order.getCurrQty() == 0;
    match_result.addFill(filltime,
        order.getTicker(),
        book_lim->order.getOrderID(), 
        book_lim->order.getPrice(),
        fill_qty,
        book_lim_filled,
        book_lim->order.getUserID()
    );
    match_result.addFill(
        filltime,
        order.getTicker(),
        order.getOrderID(),
        book_lim->order.getPrice(),
        fill_qty,
        order_filled,
        book_lim->order.getUserID()
    );
}

bool noMatchingLevel(bidbook&, uint64_t order_price, uint64_t bid_price) {
    return order_price > bid_price;
}

bool noMatchingLevel(askbook&, uint64_t order_price, uint64_t ask_price) {
    return order_price < ask_price;
}

template std::optional<MatchResult> FIFOMatch<bidbook>(Order&, bidbook&, limitbook&);
template std::optional<MatchResult> FIFOMatch<askbook>(Order&, askbook&, limitbook&);
}
}
