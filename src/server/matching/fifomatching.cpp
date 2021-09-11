#include "fifomatching.hpp"

namespace server {
namespace matching { 
template <typename T>
MatchResult FIFOMatch(Order& order_to_match, T& book, limitbook& limitbook) {
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
            addFillsAndEraseLimits(match_result, limitbook, order_to_match, book_lim, fill_qty);
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

void addFillsAndEraseLimits(MatchResult& match_result, limitbook& limitbook, 
Order& order, Limit* book_lim, uint16_t fill_qty) {
    int64_t filltime = util::getUnixTimestamp();
    bool order_filled = order.getCurrQty() == 0;
    bool book_lim_filled = book_lim->order.getCurrQty() == 0;
    match_result.addFill(
        filltime,
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
    if (book_lim_filled)
        limitbook.erase(book_lim->order.getOrderID());
}

bool comparePrices(bidbook&, uint64_t order_price, uint64_t bid_price) {
    return order_price > bid_price;
}

bool comparePrices(askbook&, uint64_t order_price, uint64_t ask_price) {
    return order_price < ask_price;
}

template MatchResult FIFOMatch<bidbook>(Order&, bidbook&, limitbook&);
template MatchResult FIFOMatch<askbook>(Order&, askbook&, limitbook&);
}
}
