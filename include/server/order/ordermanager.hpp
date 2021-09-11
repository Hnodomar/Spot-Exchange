#ifndef ORDER_MANAGER_HPP
#define ORDER_MANAGER_HPP

#include <unordered_map>
#include <vector>

#include "order.hpp"
#include "orderbook.hpp"
#include "orderresult.hpp"

namespace server {
namespace tradeorder {
class OrderManager {
public:
    OrderResult addOrder(::tradeorder::Order&& order);
    OrderResult modifyOrder(info::ModifyOrder& modify_order);
    OrderResult cancelOrder(info::CancelOrder& cancel_order);
private:
    using order_id = uint64_t; using ticker = uint64_t;
    using book_index = uint64_t;
    std::unordered_map<ticker, OrderBook> orderbooks_;
};
}
}
#endif
