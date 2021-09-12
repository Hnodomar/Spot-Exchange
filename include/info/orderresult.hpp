#ifndef ORDER_RESULT_HPP
#define ORDER_RESULT_HPP

#include <optional>
#include <cstring>

#include "matchresult.hpp"
#include "orderstatus.hpp"

namespace info {
enum class RejectionReason {
    unknown,
    order_not_found,
    order_id_already_present,
    orderbook_not_found,
    ticker_not_found,
    modify_wrong_side,
    modification_trivial,
    wrong_user_id,
    no_rejection
};
enum class OrderStatusPresent {
    NewOrderPresent,
    ModifyOrderPresent,
    CancelOrderPresent,
    RejectionPresent,
    NoStatusPresent
};
struct OrderResult {
    using MatchResult = server::matching::MatchResult;
    OrderResult(RejectionReason reason)
    : order_status_present(OrderStatusPresent::RejectionPresent) {
        orderstatus.rejection = reason;
    }
    OrderResult() : order_status_present(OrderStatusPresent::NoStatusPresent) {}
    std::optional<MatchResult> match_result;
    union OrderStatus { // C++11 allows pod-ish structs in union
        info::NewOrderStatus new_order_status;
        info::ModifyOrderStatus modify_order_status;
        info::CancelOrderStatus cancel_order_status;
        RejectionReason rejection;
        OrderStatus() {} // must allocate for pod structs w/ non-trivial constructors
    } orderstatus;
    OrderStatusPresent order_status_present;
};
}

#endif
