#ifndef CLIENT_ORDERBOOK_HPP
#define CLIENT_ORDERBOOK_HPP

#include <map>
#include <string>
#include <cstdint>
#include <iostream>

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
        auto tkr = util::convertEightBytesToString(book.ticker_);
        auto biditr = book.bids_.rbegin();
        auto askitr = book.asks_.rbegin();
        int i = 0;
        out << "=== " << tkr << " BIDS ===" << std::endl;
        while (i < 5 || biditr != book.bids_.rend()) {
            out << biditr->first << ": " << biditr->second << std::endl;
            ++i;
            ++biditr;
        }
        i = 0;
        out << "=== " << tkr << " ASKS ===" << std::endl;
        while (i < 5 || askitr != book.bids_.rend()) {
            out << askitr->first << ": " << askitr->second << std::endl;
            ++i;
            ++askitr;
        }
        return out;
    }
private:
    uint64_t ticker_;
    std::map<price, size> asks_, bids_;
};
}

#endif
