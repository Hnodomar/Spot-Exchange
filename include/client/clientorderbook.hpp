#ifndef CLIENT_ORDERBOOK_HPP
#define CLIENT_ORDERBOOK_HPP

#include <map>
#include <string>
#include <cstdint>
#include <iostream>
#include <iomanip>

#include "marketdatatypes.hpp"

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
        auto width = util::getTerminalWidth();
        auto tkr = util::convertEightBytesToString(book.ticker_);
        out << setw(width);
        string line(width, ' ');
        size_t len = line.length();
        string bidstr = "=== " + tkr + " BIDS ===";
        string askstr = "=== " + tkr + " ASKS ===";
        size_t bid_posn = (len / 2) - (0.20 * width);
        size_t ask_posn = (len / 2) + (0.10 * width);
        bid_posn += (bidstr.length() / 4);
        ask_posn += (askstr.length() / 4);

        line.replace(bid_posn, bidstr.length(), bidstr);
        line.replace(ask_posn, askstr.length(), askstr);

        bidstr = line.substr(0, ask_posn - 1);
        askstr = line.substr(ask_posn);
        out << "\033[32m" << "\033[1m" << bidstr << "\033[31m" << askstr << "\033[0m" << endl;
        line = string(width, ' ');
        --bid_posn;
        --ask_posn;

        int i = 0;
        int adjustment = 0; // to ensure asks are properly aligned.. unsure how to solve original problem
        auto biditr = book.bids_.rbegin();
        auto askitr = book.asks_.begin();
        while (i < 5) {
            if (biditr != book.bids_.rend()) {
                string entry = "£" + to_string(biditr->first) + ": " + to_string(biditr->second);
                line.replace(bid_posn, entry.length() - 1, entry);
                ++biditr;
                adjustment = 0;
            }
            else adjustment = -1;
            if (askitr != book.asks_.end()) {
                string entry = "£" + to_string(askitr->first) + ": " + to_string(askitr->second);
                line.replace(ask_posn + adjustment, entry.length(), entry);
                ++askitr;
            }
            ++i;
            out << line << endl;
            line = string(width, ' ');
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
