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
    stream_cancellation_callback_ = [this](bool success){this->onStreamCancelled(success);};
    sendResponseFromQueue_cb_ = [this](bool success){this->sendResponseFromQueue(success);};
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

thread_local OEResponseType OrderEntryStreamConnection::neworder_ack; 
thread_local OEResponseType OrderEntryStreamConnection::modorder_ack; 
thread_local OEResponseType OrderEntryStreamConnection::cancelorder_ack; 
thread_local OEResponseType OrderEntryStreamConnection::rejection_ack;
std::atomic<uint64_t> OrderEntryStreamConnection::orderid_generator_ = 1;

void OrderEntryStreamConnection::terminateConnection() {
    std::cout << "Connection terminated\n";
    client_streams_.erase(userid_);
    delete this;
}

void OrderEntryStreamConnection::asyncOpStarted() {
    ++current_async_ops_;
}

void OrderEntryStreamConnection::onStreamCancelled(bool) {
    std::cout << "stream cancelled async ops: " << current_async_ops_ << std::endl;
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
        processEntry();
        asyncOpStarted();
        grpc_responder_.Read(&oe_request_, &read_orderentry_callback_); // todo: put to back of queue
    }
}

void OrderEntryStreamConnection::writeToClient(const OEResponseType* response) {
    std::lock_guard<std::mutex> lock(response_queue_mutex_);
    response_queue_.push_back(*response);
    if (!write_in_progress_) {
        asyncOpStarted();
        grpc_responder_.Write(*response, &sendResponseFromQueue_cb_);
        write_in_progress_ = true;
    }
}

void OrderEntryStreamConnection::sendResponseFromQueue(bool success) {
    asyncOpFinished();
    std::lock_guard<std::mutex> lock(response_queue_mutex_);
    response_queue_.pop_front();
    if (!response_queue_.empty() && success) {
        asyncOpStarted();
        grpc_responder_.Write(response_queue_.front(), &sendResponseFromQueue_cb_);
    }
    else
        write_in_progress_ = false;
}

void OrderEntryStreamConnection::processEntry() {
    auto order_type = oe_request_.OrderEntryType_case();
    using type = OERequestType::OrderEntryTypeCase;
    switch(order_type) {
        case type::kNewOrder:
            oe_request_.mutable_new_order()->mutable_order_common()->set_order_id(++orderid_generator_);
            handleOrderType(oe_request_.new_order());
            break;
        case type::kModifyOrder:
            handleOrderType(oe_request_.modify_order());
            break;
        case type::kCancelOrder:
            handleOrderType(oe_request_.cancel_order());
            break;
        default:
            break;
    }
}

void OrderEntryStreamConnection::processOrderEntry(const orderentry::NewOrder& new_order) {
    using namespace tradeorder;
    const auto& order_common = new_order.order_common();
    Order order(
        new_order.is_buy_side(),
        this,
        new_order.price(),
        new_order.quantity(),
        info::OrderCommon(
            order_common.order_id(),
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
    asyncOpStarted();
    writeToClient(&rejection_ack);
}

void OrderEntryStreamConnection::processOrderEntry(const orderentry::ModifyOrder& modify_order) {
    using namespace info;
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
    std::cout << "process order entry" << std::endl;
    modify_order_fn_(morder);
}

void OrderEntryStreamConnection::processOrderEntry(const orderentry::CancelOrder& cancel_order) {
    using namespace info;
    auto order_common = cancel_order.order_common();
    info::CancelOrder corder(
        order_common.order_id(),
        order_common.user_id(),
        order_common.ticker(),
        this
    );
    cancel_order_fn_(corder);
}

bool OrderEntryStreamConnection::userIDUsageRejection(const orderentry::OrderCommon& common) {
    if (common.user_id() != userid_) {
        auto rejection = rejection_ack.mutable_rejection();
        auto rejection_common = rejection->mutable_order_common();
        *rejection_common = common;
        rejection->set_rejection_response(orderentry::OrderEntryRejection::wrong_user_id);
        writeToClient(&rejection_ack);
        return true;
    }
    return false;
}

void OrderEntryStreamConnection::acknowledgeEntry(const orderentry::NewOrder& new_order) {
    auto status = neworder_ack.mutable_new_order_ack();
    *status->mutable_new_order() = new_order;
    *status->mutable_new_order()->mutable_order_common() = new_order.order_common();
    status->set_timestamp(util::getUnixTimestamp());
    writeToClient(&neworder_ack);
}

void OrderEntryStreamConnection::acknowledgeEntry(const orderentry::ModifyOrder& modify_order) {
    auto status = modorder_ack.mutable_modify_order_ack();
    *status->mutable_modify_order() = modify_order;
    *status->mutable_modify_order()->mutable_order_common() = modify_order.order_common();
    status->set_timestamp(util::getUnixTimestamp());
    writeToClient(&modorder_ack);
}

void OrderEntryStreamConnection::acknowledgeEntry(const orderentry::CancelOrder& cancel_order) {
    auto status = cancelorder_ack.mutable_cancel_order_ack();
    *(status->mutable_status_common()) = cancel_order.order_common();
    status->set_timestamp(util::getUnixTimestamp());
    writeToClient(&cancelorder_ack);
}
