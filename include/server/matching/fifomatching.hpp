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
using order_id = uint64_t;
using Level = server::tradeorder::Level;
using Order = ::tradeorder::Order;
using Limit = server::tradeorder::Limit;
using askbook = std::map<price, Level>;
using bidbook = std::map<price, Level, std::greater<price>>;
using limitbook = std::unordered_map<order_id, Limit>;

bool comparePrices(bidbook&, uint64_t order_price, uint64_t bid_price);
bool comparePrices(askbook&, uint64_t order_price, uint64_t ask_price);
void addFillsAndEraseLimits(MatchResult& match_result, limitbook& limitbook, Order& order,
    Limit* book_lim, uint16_t fill_qty);
template <typename T>
MatchResult FIFOMatch(Order& order_to_match, T& book, limitbook& limitbook);

}
}

#endif
