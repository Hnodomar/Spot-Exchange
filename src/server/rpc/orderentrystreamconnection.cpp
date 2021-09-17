#include "orderentrystreamconnection.hpp"

OrderEntryStreamConnection::OrderEntryStreamConnection(ServiceType* service, 
grpc::ServerCompletionQueue* completion_q, std::unordered_map<uint64_t, OrderEntryStreamConnection*>& client_streams,
OEJobHandlers& job_handlers)
    : service_(service)
    , completion_queue_(completion_q)
    , grpc_responder_(&server_context_)
    , client_streams_(client_streams)
    , server_stream_done_(false)
    , on_streamcancelled_called_(false) 
{
    add_order_fn_ = job_handlers.add_order_fn;
    modify_order_fn_ = job_handlers.modify_order_fn;
    cancel_order_fn_ = job_handlers.cancel_order_fn;
    create_new_conn_fn_= job_handlers.create_new_conn_fn;
    initialise_oe_conn_callback_ = [this](bool success){this->initialiseOEConn(success);};
    read_orderentry_callback_ = [this](bool success){this->readOrderEntryCallback(success);};
    null_callback_ = [this](bool){this->asyncOpFinished();};
    verify_userid_callback_ = [this](bool success){this->verifyID(success);};
    process_neworder_entry_callback_ = [this](bool success){this->processNewOrderEntry(success);};
    process_modifyorder_entry_callback_ = [this](bool success){this->processModifyOrderEntry(success);};
    process_cancelorder_entry_callback_ = [this](bool success){this->processCancelOrderEntry(success);};
    write_from_queue_callback_ = [this](bool success){this->writeFromQueue(success);};
    stream_cancellation_callback_ = [this](bool success){this->onStreamCancelled(success);};
    server_context_.AsyncNotifyWhenDone(&stream_cancellation_callback_); // to get notification when request cancelled
    asyncOpStarted();
    service_->RequestOrderEntry(
        &server_context_,
        &grpc_responder_,
        completion_queue_,
        completion_queue_,
        &initialise_oe_conn_callback_
    );
}

thread_local OEResponseType neworder_ack; 
thread_local OEResponseType modifyorder_ack;
thread_local OEResponseType cancelorder_ack;
thread_local OEResponseType rejection_ack;
std::atomic<uint64_t> OrderEntryStreamConnection::orderid_generator_ = 1;

void OrderEntryStreamConnection::terminateConnection() {
    client_streams_.erase(userid_);
    delete this;
}

void OrderEntryStreamConnection::asyncOpStarted() {
    ++current_async_ops_;
}

void OrderEntryStreamConnection::onStreamCancelled(bool) {
    on_streamcancelled_called_ = true;
    if (current_async_ops_ == 0) {
        terminateConnection();
    }
}

void OrderEntryStreamConnection::asyncOpFinished() {
    --current_async_ops_;
    if (on_streamcancelled_called_ && current_async_ops_ == 0) {
        terminateConnection();
    }
}

void OrderEntryStreamConnection::verifyID(bool success) {
    using type = OERequestType::OrderEntryTypeCase;
    uint64_t userid = 0;
    switch(oe_request_.OrderEntryType_case()) { // slight inefficiency on first conn request
        case type::kNewOrder: {
            const auto& common = oe_request_.new_order().order_common();
            userid = common.user_id();
            break;
        }
        case type::kModifyOrder: {
            const auto& common = oe_request_.modify_order().order_common();
            userid = common.user_id();
            break;
        }
        case type::kCancelOrder: {
            const auto& common = oe_request_.cancel_order().order_common();
            userid = common.user_id();
            break;
        }
        default:
            break;
    }
    if (client_streams_.find(userid) != client_streams_.end()) {
        return;
    }
    client_streams_.emplace(userid, this);
    userid_ = userid;
    readOrderEntryCallback(success);
}

void OrderEntryStreamConnection::initialiseOEConn(bool success) {
    create_new_conn_fn_();
    asyncOpFinished();
    if (success) {
        asyncOpStarted();
        grpc_responder_.Read(&oe_request_, &verify_userid_callback_);
    }
}

void OrderEntryStreamConnection::readOrderEntryCallback(bool success) {
    asyncOpFinished();
    if (success) {
        acknowledgeEntry();
        asyncOpStarted();
        grpc_responder_.Read(&oe_request_, &read_orderentry_callback_);
    }
}

void OrderEntryStreamConnection::writeFromQueue(bool success) {
    asyncOpFinished();
    std::lock_guard<std::mutex> lock(this->response_queue_mutex_);
    response_queue_.pop_front();
    if (success && !response_queue_.empty()) {
        asyncOpStarted();
        grpc_responder_.Write(response_queue_.front(), &write_from_queue_callback_);
    }
}

void OrderEntryStreamConnection::queueWrite(const OEResponseType* response) {
    std::lock_guard<std::mutex> lock(this->response_queue_mutex_);
    response_queue_.push_back(std::move(*response));
    asyncOpStarted();
    alarm_.Set(completion_queue_, gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME), &write_from_queue_callback_);
}

void OrderEntryStreamConnection::acknowledgeEntry() {
    auto order_type = oe_request_.OrderEntryType_case();
    using type = OERequestType::OrderEntryTypeCase;
    switch(order_type) {
        case type::kNewOrder: {
            const auto& new_order = oe_request_.new_order();
            if (userIDUsageRejection(new_order.order_common()))
                return;
            sendNewOrderAcknowledgement(new_order);
            break;
        }
        case type::kModifyOrder: {
            const auto& modify_order = oe_request_.modify_order();
            if (userIDUsageRejection(modify_order.order_common()))
                return;
            sendModifyOrderAcknowledgement(modify_order);
            break;
        }
        case type::kCancelOrder: {
            const auto& cancel_order = oe_request_.cancel_order();
            if (userIDUsageRejection(cancel_order.order_common()))
                return;
            sendCancelOrderAcknowledgement(cancel_order);
            break;
        }
        default:
            break;
    }
}

void OrderEntryStreamConnection::processNewOrderEntry(bool success) {
    using namespace tradeorder;
    const auto& new_order = oe_request_.new_order();
    const auto& order_common = new_order.order_common();
    Order order(
        new_order.is_buy_side(),
        this,
        new_order.price(),
        new_order.quantity(),
        info::OrderCommon(
            ++orderid_generator_,
            order_common.user_id(),
            order_common.ticker()
        )
    );
    add_order_fn_(order);
}

void OrderEntryStreamConnection::sendRejection(const Rejection rejection, const uint64_t userid,
const uint64_t orderid, const uint64_t ticker) {
    auto rejection_obj = rejection_ack.mutable_rejection();
    rejection_obj->set_rejection_response(rejection);
    auto common_obj = rejection_obj->mutable_order_common();
    common_obj->set_user_id(userid);
    common_obj->set_order_id(orderid);
    common_obj->set_ticker(ticker);
    grpc_responder_.Write(rejection_ack, &null_callback_);
}

void OrderEntryStreamConnection::processModifyOrderEntry(bool success) {
    using namespace info;
    const auto& modify_order = oe_request_.new_order();
    const auto& order_common = modify_order.order_common();
    info::ModifyOrder morder(
        modify_order.is_buy_side(),
        this,
        modify_order.price(),
        modify_order.quantity(),
        info::OrderCommon(
            order_common.order_id(),
            order_common.user_id(),
            order_common.ticker()
        )
    );
    modify_order_fn_(morder);
}

void OrderEntryStreamConnection::processCancelOrderEntry(bool success) {
    using namespace info;
    auto cancel_order = oe_request_.cancel_order();
    auto order_common = cancel_order.order_common();
    info::CancelOrder corder(
        order_common.order_id(),
        order_common.user_id(),
        order_common.ticker(),
        this
    );
    cancel_order_fn_(corder);
}

void OrderEntryStreamConnection::sendNewOrderAcknowledgement(const orderentry::NewOrder& new_order) {
    auto noa_status = neworder_ack.mutable_new_order_ack();
    noa_status->set_is_buy_side(new_order.is_buy_side());
    noa_status->set_quantity(new_order.quantity());
    noa_status->set_price(new_order.price());
    auto noa_com_status = noa_status->mutable_status_common();
    auto new_order_common = new_order.order_common();
    noa_com_status->set_order_id(new_order_common.order_id());
    noa_com_status->set_ticker(new_order_common.ticker());
    noa_com_status->set_user_id(new_order_common.user_id());
    noa_status->set_timestamp(util::getUnixTimestamp());
    std::lock_guard<std::mutex> lock(request_queue_mutex_);
    request_queue_.emplace_back(std::move(oe_request_));
    asyncOpStarted();
    grpc_responder_.Write(neworder_ack, &process_neworder_entry_callback_);
}

void OrderEntryStreamConnection::sendModifyOrderAcknowledgement(const orderentry::ModifyOrder& modify_order) {
    auto moa_status = modifyorder_ack.mutable_modify_order_ack();
    moa_status->set_is_buy_side(modify_order.is_buy_side());
    moa_status->set_quantity(modify_order.quantity());
    moa_status->set_price(modify_order.price());
    auto moa_com_status = moa_status->mutable_status_common();
    auto modify_order_common = modify_order.order_common();
    moa_com_status->set_order_id(modify_order_common.order_id());
    moa_com_status->set_ticker(modify_order_common.ticker());
    moa_com_status->set_user_id(modify_order_common.user_id());
    moa_status->set_timestamp(util::getUnixTimestamp());
    std::lock_guard<std::mutex> lock(request_queue_mutex_);
    request_queue_.emplace_back(std::move(oe_request_));
    asyncOpStarted();
    grpc_responder_.Write(modifyorder_ack, &process_modifyorder_entry_callback_);
}

void OrderEntryStreamConnection::sendCancelOrderAcknowledgement(const orderentry::CancelOrder& cancel_order) {
    auto coa_status = cancelorder_ack.mutable_cancel_order_ack();
    auto coa_com_status = coa_status->mutable_status_common();
    auto cancel_order_common = cancel_order.order_common();
    coa_com_status->set_order_id(cancel_order_common.order_id());
    coa_com_status->set_ticker(cancel_order_common.ticker());
    coa_com_status->set_user_id(cancel_order_common.user_id());
    coa_status->set_timestamp(util::getUnixTimestamp());
    std::lock_guard<std::mutex> lock(request_queue_mutex_);
    request_queue_.emplace_back(std::move(oe_request_));
    asyncOpStarted();
    grpc_responder_.Write(cancelorder_ack, &process_cancelorder_entry_callback_);
}

bool OrderEntryStreamConnection::userIDUsageRejection(const orderentry::OrderCommon& common) {
    if (common.user_id() != userid_) {
        auto rejection = rejection_ack.mutable_rejection();
        auto rejection_common = rejection->mutable_order_common();
        rejection_common->set_user_id(common.user_id());
        rejection_common->set_ticker(common.ticker());
        rejection_common->set_order_id(common.order_id());
        rejection->set_rejection_response(orderentry::OrderEntryRejection::wrong_user_id);
        asyncOpStarted();
        grpc_responder_.Write(rejection_ack, &null_callback_);
        return true;
    }
    return false;
}
