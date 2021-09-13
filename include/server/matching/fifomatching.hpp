#ifndef FIFO_MATCHING_HPP
#define FIFO_MATCHING_HPP

#include <map>
#include <functional>
#include <optional>

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
void addFills(MatchResult& match_result, Order& order, Limit* book_lim, uint32_t fill_qty);
void popLimitFromQueue(Limit*& book_lim, Level& book_lvl, limitbook& limitbook);
template<typename Book, typename BookItr>
inline bool orderFullyMatchedInLevel(Limit*& book_lim, Order& order_to_match, MatchResult& match_result, 
Level& book_lvl, limitbook& limitbook, Book& book, BookItr& book_itr);
template <typename T>
std::optional<MatchResult> FIFOMatch(Order& order_to_match, T& book, limitbook& limitbook);

}
}

#endif
