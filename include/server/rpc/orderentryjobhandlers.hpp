#ifndef ENTRY_HANDLERS_HPP
#define ENTRY_HANDLERS_HPP

#include <functional>

namespace tradeorder {
    class Order;
}

namespace info {
    struct OrderCommon;
    struct ModifyOrder;
    struct CancelOrder;
}

struct OEJobHandlers {
    std::function<void(::tradeorder::Order&)> add_order_fn;
    std::function<void(info::ModifyOrder&)> modify_order_fn;
    std::function<void(info::CancelOrder&)> cancel_order_fn;
    std::function<void()> create_new_conn_fn;
    std::function<void()> set_context_fn;
};

#endif
