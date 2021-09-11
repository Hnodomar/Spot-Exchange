#ifndef ORDER_RESULT_HPP
#define ORDER_RESULT_HPP

#include "matchresult.hpp"
namespace info {
enum RejectionReason {
    unknown = 0,
    order_not_found = 1,
    order_id_already_present = 2,
    orderbook_not_found = 3,
    ticker_not_found = 4,
    modify_wrong_side = 5,
    modification_trivial = 6,
    wrong_user_id = 7,
    no_rejection = 8
};
struct OrderResult {
    OrderResult(RejectionReason reason) : rejection(reason) {}
    OrderResult() : rejection(RejectionReason::no_rejection) {}
    server::matching::MatchResult match_result;
    RejectionReason rejection;
};
}

#endif
