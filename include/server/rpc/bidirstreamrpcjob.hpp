#ifndef BIDIRECTIONAL_RPC_JOB_HPP
#define BIDIRECTIONAL_RPC_JOB_HPP

#include <grpcpp/grpcpp.h>
#include <list>

#include "rpcjob.hpp"
#include "jobhandlers.hpp"


class OrderEntryStreamConnection : public RPCJob {
using OERequestType = orderentry::OrderEntryRequest;
using OEResponseType = orderentry::OrderEntryResponse;
using JobHandlers = BiDirectionalStreamHandler<ServiceType, OERequestType, OEResponseType>;
using TagProcessor = std::function<void(bool)>;
public:
    OrderEntryStreamConnection(ServiceType* service, grpc::ServerCompletionQueue* completion_q, 
    JobHandlers job_handlers, std::unordered_map<uint64_t, RPCJob*>& client_streams):
        service_(service), 
        completion_queue_(completion_q), 
        grpc_responder_(&server_context_),
        job_handlers_(job_handlers),
        client_streams_(client_streams),
        server_stream_done_(false),
        client_stream_done_(false),
        on_done_called_(false) {
        on_init_ = std::bind(&OrderEntryStreamConnection::onInit, this, std::placeholders::_1);
        on_read_ = std::bind(&OrderEntryStreamConnection::onRead, this, std::placeholders::_1);
        on_write_ = std::bind(&OrderEntryStreamConnection::onWrite, this, std::placeholders::_1);
        on_finish_ = std::bind(&OrderEntryStreamConnection::onFinish, this, std::placeholders::_1);
        on_done_ = std::bind(&RPCJob::onDone, this, std::placeholders::_1);
        verify_userid_ = std::bind(&OrderEntryStreamConnection::verifyID, this, std::placeholders::_1);
        server_context_.AsyncNotifyWhenDone(&on_done_); // to get notification when request cancelled
        send_response_handler_ = std::bind(&OrderEntryStreamConnection::sendResponse, this, std::placeholders::_1);
        job_handlers_.rpcJobContextHandler(service_, this, &server_context_, send_response_handler_);
        asyncOperationStarted(RPCJob::AsyncOperationType::QUEUED_REQUEST);
        job_handlers_.queueReqHandler(
            service_, 
            &server_context_, 
            &grpc_responder_, 
            completion_queue_, 
            completion_queue_,
            &on_init_
        );
    }   
    const uint64_t getUserID() const {return userid_;}
private:
    void onInit(bool ok) {
        job_handlers_.createRPCJobHandler();
        if (asyncOperationFinished(RPCJob::AsyncOperationType::QUEUED_REQUEST)) {
            if (ok) {
                ++current_async_ops_;
                asyncOperationStarted(RPCJob::AsyncOperationType::READ);
                grpc_responder_.Read(&request_, &verify_userid_); // put to back of cq
            }
        }
    }
    void verifyID(bool ok) { // perform user id verification on first conn request
        using type = OERequestType::OrderEntryTypeCase;
        uint64_t userid = 0;
        switch(request_.OrderEntryType_case()) { // slight inefficiency on first conn request
            case type::kNewOrder: {
                const auto& common = request_.new_order().order_common();
                userid = common.user_id();
                break;
            }
            case type::kModifyOrder: {
                const auto& common = request_.modify_order().order_common();
                userid = common.user_id();
                break;
            }
            case type::kCancelOrder: {
                const auto& common = request_.cancel_order().order_common();
                userid = common.user_id();
                break;
            }
            default:
                break;
        }
        if (client_streams_.find(userid) != client_streams_.end()) {
            return;
        }
        userid_ = userid;
        onRead(ok);
    }
    void onRead(bool ok) {
        if (asyncOperationFinished(RPCJob::AsyncOperationType::READ)) {
            if (ok) {
                job_handlers_.processRequestHandler(this, &request_);
                asyncOperationStarted(RPCJob::AsyncOperationType::READ);
                grpc_responder_.Read(&request_, &on_read_); // put to back of cq
            }
            else {
                client_stream_done_ = true;
                //job_handlers_.processRequestHandler(service_, this, nullptr);
            }
        }
    }
    void onWrite(bool ok) {
        if (asyncOperationFinished(RPCJob::AsyncOperationType::WRITE)) {
            response_queue_.pop_front();
            if (ok) {
                if (!response_queue_.empty()) {
                    writeResponse();
                }
                else if (server_stream_done_) {
                    finish();
                }
            }
        }
    }
    void onFinish(bool ok) {
        asyncOperationFinished(RPCJob::AsyncOperationType::FINISH);
    }
    void done() override {
        job_handlers_.rpcJobDoneHandler(service_, this, server_context_.IsCancelled());
    }
    bool sendResponse(const OEResponseType* response) {
        if (response == nullptr && !client_stream_done_) {
            GPR_ASSERT(false);
            return false;
        }
        if (response != nullptr) {
            response_queue_.push_back(*response);
            if (!isAsyncWriteInProgress())
                writeResponse();
        }
        else {
            server_stream_done_ = true;
            if (!isAsyncWriteInProgress())
                finish();
        }
        return true;
    }
    void writeResponse() {
        asyncOperationStarted(RPCJob::AsyncOperationType::WRITE);
        grpc_responder_.Write(response_queue_.front(), &on_write_);
    }
    void finish() {
        asyncOperationStarted(RPCJob::AsyncOperationType::FINISH);
        grpc_responder_.Finish(grpc::Status::OK, &on_finish_);
    }
    bool asyncOpStarted() {
        ++current_async_ops_;
    }
    bool asyncOpFinished() {
        --current_async_ops_;
        if (current_async_ops_ == 0 && on_done_called_)
            done();
    }
    void onDone(bool) { // notification tag callback for stream termination
        on_done_called_ = true;
        if (current_async_ops_ == 0)
            done();
    }
    ServiceType* service_;
    grpc::ServerCompletionQueue* completion_queue_;
    grpc::ServerContext server_context_;
    OERequestType request_;
    typename JobHandlers::GRPCResponder grpc_responder_;
    JobHandlers job_handlers_;
    typename JobHandlers::SendResponseHandler send_response_handler_;
    TagProcessor on_init_;
    TagProcessor on_read_;
    TagProcessor on_write_;
    TagProcessor on_finish_;
    TagProcessor verify_userid_;
    TagProcessor on_done_;
    TagProcessor acknowledge_entry_;
    TagProcessor process_entry_;
    TagProcessor notify_client_entry_result_;
    TagProcessor inform_data_platform_;
    std::list<OEResponseType> response_queue_;
    std::unordered_map<uint64_t, RPCJob*>& client_streams_;
    uint64_t userid_;
    int64_t current_async_ops_;
    bool server_stream_done_;
    bool client_stream_done_;
    bool on_done_called_;
};

#endif
