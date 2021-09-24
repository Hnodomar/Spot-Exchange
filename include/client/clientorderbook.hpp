#ifndef CLIENT_ORDERBOOK_HPP
#define CLIENT_ORDERBOOK_HPP

#include <map>
#include <string>
#include <cstdint>
#include <iostream>
#include <iomanip>

#include "marketdatatypes.hpp"
#include "centerformatting.hpp"

namespace client {
using price = uint64_t; using size = int64_t;
class ClientOrderBook {
public:
    ClientOrderBook() = default;
    ClientOrderBook(uint64_t ticker) : ticker_(ticker) {}
    void addToBook(uint8_t is_buy_side, int32_t shares, uint64_t price) {
        if (is_buy_side) bids_[price] += shares;
        else asks_[price] += shares;
    }
    void removeFromBook(uint8_t is_buy_side, int32_t shares, uint64_t price) {
        auto& book_side = (is_buy_side ? bids_ : asks_);
        auto itr = book_side.find(price);
        if (itr == book_side.end())
            return;
        itr->second -= shares;
        if (itr->second <= 0)
            book_side.erase(itr);
    }
    friend std::ostream& operator<<(std::ostream& out, const ClientOrderBook& book) {
        using namespace std;
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        out << std::setfill(' ') << std::setw(w.ws_col);
        auto tkr = util::convertEightBytesToString(book.ticker_);
        auto biditr = book.bids_.rbegin();
        auto askitr = book.asks_.begin();
        int i = 0;
        out << std::setw(w.ws_col);
        std::string line(w.ws_col, ' ');
        std::size_t len = line.length();
        std::string bidstr = "=== " + tkr + " BIDS ===";
        std::string askstr = "=== " + tkr + " ASKS ===";
        std::size_t bid_posn = (len / 2) - (0.20 * w.ws_col);
        std::size_t ask_posn = (len / 2) + (0.10 * w.ws_col);
        line.insert(bid_posn, bidstr);
        line.insert(ask_posn, askstr);
        out << centered(line.substr(0, w.ws_col)) << std::endl;
        line = std::string(w.ws_col, ' ');
        bid_posn += bidstr.length() / 4;
        ask_posn += askstr.length() / 4;
        while (i < 5) {
            if (biditr != book.bids_.rend()) {
                line.insert(bid_posn, "£" + to_string(biditr->first) + ": " + to_string(biditr->second));
                ++biditr;
            }
            if (askitr != book.asks_.end()) {
                line.insert(ask_posn, "£" + to_string(askitr->first) + ": " + to_string(askitr->second));
                ++askitr;
            }
            ++i;
            out << std::setw(w.ws_col) << line.substr(0, w.ws_col) << std::endl;
            line = std::string(w.ws_col, ' ');
        }
        return out;
    }
    uint64_t getTicker() const {return ticker_;}
private:
    uint64_t ticker_;
    std::map<price, size> asks_, bids_;
};
}

#endif
