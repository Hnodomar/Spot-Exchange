#ifndef FIFO_MATCHING_HPP
#define FIFO_MATCHING_HPP

#include <map>
#include <functional>
#include <optional>

#include "order.hpp"
#include "limit.hpp"
#include "level.hpp"
#include "matchresult.hpp"
#include "util.hpp"

using order_id = uint64_t;
using limitbook = std::unordered_map<order_id, server::tradeorder::Limit>;

namespace server {
namespace matching {
using Order = ::tradeorder::Order;
using price = uint64_t;
using Level = server::tradeorder::Level;
using Limit = server::tradeorder::Limit;
using askbook = std::map<price, Level>;
using bidbook = std::map<price, Level, std::greater<price>>;

class FIFOMatcher {
public:
    template<typename Book>
    static MatchResult FIFOMatch(Order& order_to_match, Book& book, limitbook& limitbook) {
        auto book_itr = book.begin();
        Level& book_lvl = book_itr->second;
        Limit* book_order = book_lvl.head;
        uint64_t order_price = order_to_match.getPrice();
        MatchResult match_result;
        while (book_itr != book.end()) {
            if (noMatchingLevel(book, order_price, book_itr->first)) {
                return match_result;
            }
            book_order = book_lvl.head;
            if (orderFullyMatchedInLevel(book_order, order_to_match, match_result, book_lvl, limitbook, book, book_itr)) {
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
private:
    template<typename Book, typename BookItr>
    static bool orderFullyMatchedInLevel(Limit*& book_order, Order& order_to_match, MatchResult& match_result, 
    Level& book_lvl, limitbook& limitbook, Book& book, BookItr& book_itr) {
        while (ordersInLevel(book_order)) {
            uint32_t fill_qty = std::min(book_order->order.getCurrQty(), order_to_match.getCurrQty());
            order_to_match.decreaseQty(fill_qty);
            book_order->order.decreaseQty(fill_qty);
            addFills(match_result, order_to_match, book_order, fill_qty);
            if (orderIsFullyMatched(order_to_match)) { // order_to_match is fully matched
                if (book_order->order.getCurrQty() == 0) {
                    removeOrderFromBook(book_order, book_lvl, limitbook);
                }
                if (book_lvl.getLevelOrderCount() == 0) {
                    book.erase(book_itr++);
                }
                match_result.setOrderFilled();
                return true;
            }
            removeOrderFromBook(book_order, book_lvl, limitbook);
        }
        return false;
    }
    static void removeOrderFromBook(Limit*& book_lim, Level& book_lvl, limitbook& limitbook) {
        Limit* next_limit = book_lim->next_limit;
        if (next_limit != nullptr) {
            next_limit->prev_limit = nullptr;
        }
        book_lvl.head = next_limit;
        uint64_t book_lim_id = book_lim->order.getOrderID();
        book_lim = next_limit;
        limitbook.erase(book_lim_id);
    }
    static void addFills(MatchResult& match_result, Order& order, Limit* book_lim, uint32_t fill_qty) {
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
            book_lim->order.getUserID(),
            order.connection_
        );
        match_result.addFill(
            filltime,
            order.getTicker(),
            order.getOrderID(),
            book_lim->order.getPrice(),
            fill_qty,
            order_filled,
            book_lim->order.getUserID(),
            order.connection_
        );
    }
    static bool noMatchingLevel(bidbook&, uint64_t order_price, uint64_t bid_price) {
        return order_price > bid_price;
    }
    static bool noMatchingLevel(askbook&, uint64_t order_price, uint64_t ask_price) {
        return order_price < ask_price;
    }
    static bool ordersInLevel(const Limit* order) {return order != nullptr;}
    static bool orderIsFullyMatched(const Order& order_to_match) {return order_to_match.getCurrQty() == 0;}
};

}
}

#endif
