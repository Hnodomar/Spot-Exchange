#ifndef CLIENT_FEED_HANDLER_HPP
#define CLIENT_FEED_HANDLER_HPP

#include <unordered_map>
#include <vector>

#include "clientorderbook.hpp"
#include "marketdatatypes.hpp"

namespace client {
using Ticker = uint64_t;
using BookIndex = uint16_t;
using OrderID = uint64_t;
using Order = AddOrderData;
class ClientFeedHandler {
public:
    void addOrder(AddOrderData* new_order) {
        auto itr = tickers_.find(new_order->ticker);
        if (itr == tickers_.end()) {
            orderbooks_.emplace_back(ClientOrderBook());
            itr = tickers_.emplace(new_order->ticker, orderbooks_.size() - 1).first;
        }
        uint16_t book_id = itr->second;
        new_order->book_index = book_id;
        auto& book = orderbooks_[book_id];
        book.addToBook(new_order->is_buy_side, new_order->quantity, new_order->price);
        orders_.emplace(new_order->order_id, std::move(*new_order));
    }
    void cancelOrder(CancelOrderData* cancel_order) {
        auto order_itr = orders_.find(cancel_order->order_id);
        if (order_itr == orders_.end())
            return;
        auto& book = orderbooks_[order_itr->second.book_index];
        book.removeFromBook(
            order_itr->second.is_buy_side,
            order_itr->second.quantity,
            order_itr->second.price
        );
        orders_.erase(order_itr);
    }
    void modifyOrder(ModOrderData* modify_order) {
        auto itr = orders_.find(modify_order->order_id);
        if (itr == orders_.end())
            return;
        auto& book = orderbooks_[itr->second.book_index];
        if (modify_order->quantity > itr->second.quantity) {
            book.addToBook(
                itr->second.is_buy_side,
                modify_order->quantity - itr->second.quantity,
                itr->second.price
            );
        }
        else {
            book.removeFromBook(
                itr->second.is_buy_side,
                itr->second.quantity - modify_order->quantity,
                itr->second.price
            );
        }
        itr->second.quantity = modify_order->quantity;
    }
    void fillOrder(FillOrderData* fill) {
        auto itr = orders_.find(fill->order_id);
        if (itr == orders_.end())
            return;
        auto& book = orderbooks_[itr->second.book_index];
        book.removeFromBook(
            itr->second.is_buy_side,
            fill->quantity,
            itr->second.price
        );
        itr->second.quantity -= fill->quantity;
        if (itr->second.quantity <= 0)
            orders_.erase(itr);
    }
    ClientOrderBook* subscribe(Ticker ticker) {
        auto itr = tickers_.find(ticker);
        if (itr == tickers_.end()) {

        }
        return orderbooks_[itr->second];
    }
private:
    std::vector<ClientOrderBook> orderbooks_;
    std::unordered_map<Ticker, BookIndex> tickers_;
    std::unordered_map<OrderID, Order> orders_;
};
}

#endif
