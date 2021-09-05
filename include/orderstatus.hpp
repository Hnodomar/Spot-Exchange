#ifndef ORDER_STATUS_HPP
#define ORDER_STATUS_HPP

#include <memory>

struct NewOrderStatus {

};

struct ModifyOrderStatus {

};

struct CancelOrderStatus {
    
};

struct OrderStatusFactory {
    static CancelOrderStatus createCancelOrderStatus() {
        return CancelOrderStatus();
    }
    static NewOrderStatus createNewOrderStatus() {
        return NewOrderStatus();
    }
    static ModifyOrderStatus createModifyOrderStatus() {
        return ModifyOrderStatus();
    }
};

#endif
