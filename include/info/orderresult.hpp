#ifndef ORDER_RESULT_HPP
#define ORDER_RESULT_HPP

#include <optional>
#include <cstring>

#include "matchresult.hpp"
#include "orderstatus.hpp"

// the templates in this file are NOT checked as being derived from the list
// of scoped enumerations also in this file (via C++11 type-traits), 
// for the sake of performance. [type-safety vs performance trade off]

namespace info {
using MatchResult = server::matching::MatchResult;
using OptMatchResult = std::optional<MatchResult>;


enum class AddOrderResultType {
    fully_matched,
    partially_matched,
    no_match,
    rejection
};

enum class AddOrderRejectionReason {
    unknown,
    order_not_found,
    order_id_already_present,
    orderbook_not_found,
    no_rejection
};

struct AddOrderResult {
    AddOrderResult(AddOrderRejectionReason rejection)
     : rejection(rejection)
     , match_result(std::nullopt)
     , type(AddOrderResultType::rejection)
    {}
    AddOrderResult(std::optional<MatchResult> match_result)
     : rejection(AddOrderRejectionReason::no_rejection)
     , match_result(match_result)
     , type(AddOrderResultType::fully_matched)
    {}
    AddOrderResult(std::optional<MatchResult> match_result, AddOrderStatus order_status)
     : rejection(AddOrderRejectionReason::no_rejection)
     , match_result(match_result)
     , type(AddOrderResultType::partially_matched)
    {}
    AddOrderResult(AddOrderStatus order_status)
     : rejection(AddOrderRejectionReason::no_rejection)
     , order_status(order_status)
     , type(AddOrderResultType::no_match)
    {}
    const std::optional<MatchResult> match_result;
    const AddOrderStatus order_status;
    const AddOrderResultType type;
    const AddOrderRejectionReason rejection;
};

enum class CancelRejectionReason {
    unknown,
    order_not_found,
    wrong_user_id,
    orderbook_not_found,
    no_rejection
};

enum class CancelOrderType {
    success,
    rejected
};

struct CancelOrderResult {
    CancelOrderResult(const CancelRejectionReason RejectionReason)
     : rejection(rejection)
     , type(CancelOrderType::rejected)
    {}
    CancelOrderResult(CancelOrderStatus order_status)
     : cancel_status(order_status)
     , type(CancelOrderType::success)
     , rejection(CancelRejectionReason::no_rejection)
    {}
    const CancelOrderType type;
    const CancelOrderStatus cancel_status;
    const CancelRejectionReason rejection;
};

using OptCancelResult = std::optional<CancelOrderResult>;
using OptAddResult = std::optional<AddOrderResult>;

enum class ModifyRejectionReason {
    unknown,
    order_not_found,
    modify_wrong_side,
    wrong_user_id,
    modification_trivial,
    orderbook_not_found,
    no_rejection
};

enum class ModifyOrderType {
    replace,
    modify,
    rejected
};  

struct ModifyOrderResult {
    ModifyOrderResult(AddOrderResult add_result, CancelOrderResult cancel_result)
     : add_result(add_result)
     , cancel_result(cancel_result)
     , rejection(ModifyRejectionReason::no_rejection)
     , type(ModifyOrderType::replace)
    {}
    ModifyOrderResult(AddOrderResult add_result)
     : add_result(add_result)
     , cancel_result(std::nullopt)
     , rejection(ModifyRejectionReason::no_rejection)
     , type(ModifyOrderType::modify)
    {}
    ModifyOrderResult(ModifyRejectionReason rejection)
     : add_result(std::nullopt)
     , cancel_result(std::nullopt)
     , rejection(rejection)
     , type(ModifyOrderType::rejected)
    {}
    const OptAddResult add_result;
    const OptCancelResult cancel_result;
    const ModifyRejectionReason rejection;
    const ModifyOrderType type;
};

}

#endif
