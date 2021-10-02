#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstdint>
#include <map>
#include <array>
#include <iostream>
#include <utility>
#include <mutex>
#ifndef TEST_BUILD
#include "orderentrystreamconnection.hpp"
#include "marketdatadispatcher.hpp"
#else 
namespace rpc {class MarketDataDispatcher;}
#endif
#include "logger.hpp"
#include "exception.hpp"
#include "fifomatching.hpp"
#include "order.hpp"
#include "limit.hpp"

static constexpr uint8_t UNKNOWN = 0;
static constexpr uint8_t ORDER_NOT_FOUND = 1;
static constexpr uint8_t ORDER_ID_ALREADY_PRESENT = 2;
static constexpr uint8_t ORDERBOOK_NOT_FOUND = 3;
static constexpr uint8_t TICKER_NOT_FOUND = 4;
static constexpr uint8_t MODIFY_WRONG_SIDE = 5;
static constexpr uint8_t MODIFICATION_TRIVIAL = 6;
static constexpr uint8_t WRONG_USER_ID = 7;

namespace server {
namespace tradeorder {
using namespace info;
using price = uint64_t;
using order_id = uint64_t;
using Order = ::tradeorder::Order;
using askbook = std::map<price, Level>;
using bidbook = std::map<price, Level, std::greater<price>>;
using limitbook = std::unordered_map<order_id, server::tradeorder::Limit>;
using MatchResult = server::matching::MatchResult;
using GetOrderResult = std::pair<bool, Order&>;
#ifndef TEST_BUILD
using Rejection = orderentry::OrderEntryRejection::RejectionReason; 
using MDResponse = orderentry::MarketDataResponse;
#else
enum Rejection {
    UNKNOWN = 1,
    ORDER_NOT_FOUND = 1,
    ORDER_ID_ALREADY_PRESENT = 2,
    ORDERBOOK_NOT_FOUND = 3,
    TICKER_NOT_FOUND = 4,
    MODIFY_WRONG_SIDE = 5,
    MODIFICATION_TRIVIAL = 6,
    WRONG_USER_ID = 7,
};
#endif

class OrderBook {
public:
    OrderBook(rpc::MarketDataDispatcher* md_dispatch);
    OrderBook(const OrderBook& orderbook);
    OrderBook() = default;
    void addOrder(Order& order);
    void modifyOrder(const info::ModifyOrder& modify_order);
    void cancelOrder(const info::CancelOrder& cancel_order);
    GetOrderResult getOrder(uint64_t order_id);
    uint64_t numOrders() const {return limitorders_.size();}
    uint64_t numLevels() const {return asks_.size() + bids_.size();}
    rpc::MarketDataDispatcher* getMDDispatcher() const {return md_dispatch_;}
private:
    template<typename BookToMatchOn, typename BookToAddTo> 
    void addOrder(Order& order, BookToMatchOn& match_book, BookToAddTo& add_book);
    template<typename Book> void placeOrderInBook(Order& order, Book& book, bool is_buy_side);
    template<typename Book> Level& getSideLevel(const uint64_t price, Book& sidebook);
    template<typename OrderType> void sendRejection(Rejection rejection, const OrderType& order);
    void processModifyError(uint8_t error_flags, const ModifyOrder& order);
    void communicateMatchResults(MatchResult& match_result, const Order& order) const;
    bool possibleMatches(const askbook& book, const Order& order) const;
    bool possibleMatches(const bidbook& book, const Order& order) const;
    bool modifyOrderTrivial(const info::ModifyOrder& modify_order, const Order& order);
    void placeLimitInBookLevel(Level& level, Order& order);
    void sendOrderAddedToDispatcher(const Order& order);
    void sendOrderCancelledToDispatcher(const info::CancelOrder& cancel_order);
    void sendOrderModifiedToDispatcher(const info::ModifyOrder& modify_order);
    bool isTailOrder(const Limit& lim) const;
    bool isHeadOrder(const Limit& lim) const;
    bool isHeadAndTail(const Limit& lim) const;
    bool isInMiddleOfLevel(const Limit& lim) const;
    
    uint64_t ticker_;
    askbook asks_;
    bidbook bids_;
    std::unordered_map<order_id, Limit> limitorders_;
    std::mutex orderbook_mutex_;
    rpc::MarketDataDispatcher* md_dispatch_;
    #ifndef TEST_BUILD
    static thread_local orderentry::OrderEntryResponse orderfill_ack;
    static thread_local MDResponse orderfill_data;
    static thread_local MDResponse neworder_data;
    static thread_local MDResponse modorder_data;
    static thread_local MDResponse cancelorder_data;
    #endif
};

template<typename OrderType>
inline void OrderBook::sendRejection(Rejection rejection, const OrderType& order) {
    #ifndef TEST_BUILD
    order.connection->sendRejection(rejection, order.user_id, order.order_id, order.ticker);
    #endif
}

template<typename Book>
inline Level& OrderBook::getSideLevel(const uint64_t price, Book& sidebook) {
    auto lvlitr = sidebook.find(price);
    if (lvlitr == sidebook.end()) {
        auto ret = sidebook.emplace(price, Level(price));
        if (!ret.second) {
            logging::Logger::Log(
                logging::LogType::Error,
                util::getLogTimestamp(),
                "Failed to emplace price level", price,
                "in orderbook", ticker_
            );
            throw EngineException("Unable to emplace price level in orderbook");
        }
        lvlitr = ret.first;
    }
    return lvlitr->second;
}
template<typename BookToMatchOn, typename BookToAddTo> 
void OrderBook::addOrder(Order& order, BookToMatchOn& match_book, BookToAddTo& add_book) {
    if (possibleMatches(match_book, order)) {
        auto match_result = matching::FIFOMatcher::FIFOMatch(order, match_book, limitorders_);
        communicateMatchResults(match_result, order);
        if (order.getCurrQty() == 0)
            return;
    }
    placeOrderInBook(order, add_book, order.isBuySide());
    sendOrderAddedToDispatcher(order);
}

template<typename Book>
inline void OrderBook::placeOrderInBook(Order& order, Book& book, bool is_buy_side) {
    Level& level = getSideLevel(order.getPrice(), book);
    level.is_buy_side = is_buy_side;
    placeLimitInBookLevel(level, order);
}

}
}

#endif
