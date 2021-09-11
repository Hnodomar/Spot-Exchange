#ifndef BIDIRECTIONAL_RPC_JOB_HPP
#define BIDIRECTIONAL_RPC_JOB_HPP

#include "rpcjob.hpp"
#include "jobhandlers.hpp"
#include <list>

template<typename ServiceType, typename RequestType, typename ResponseType>
class BiDirStreamRPCJob : public RPCJob {
    using JobHandlers = BiDirectionalStreamHandler<ServiceType, RequestType, ResponseType>;
    using TagProcessor = std::function<void(bool)>;
public:
    BiDirStreamRPCJob(ServiceType* service, grpc::ServerCompletionQueue* completion_q, JobHandlers job_handlers):
        service_(service), 
        completion_queue_(completion_q), 
        grpc_responder_(&server_context_),
        job_handlers_(job_handlers),
        server_stream_done_(false),
        client_stream_done_(false) {
        on_init_ = std::bind(&BiDirectionalRPCJob::onInit, this, std::placeholders::_1);
        on_read_ = std::bind(&BiDirectionalRPCJob::onRead, this, std::placeholders::_1);
        on_write_ = std::bind(&BiDirectionalRPCJob::onRead, this, std::placeholders::_1);
        on_finish_ = std::bind(&BiDirectionalRPCJob::onFinish, this, std::placeholders::_1);
        on_done_ = std::bind(&RPCJob::onDone, this, std::placeholders::_1);
        server_context_.AsyncNotifyWhenDone(&on_done_);
        send_response_handler_ = std::bind(&BiDirectionalRPCJob::sendResponse, this, std::placeholders::_1);
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
private:
    void onInit(bool ok) {
        job_handlers_.createRPCJobHandler();
        if (asyncOperationFinished(RPCJob::AsyncOperationType::QUEUED_REQUEST)) {
            if (ok) {
                asyncOperationStarted(RPCJob::AsyncOperationType::READ);
                grpc_responder_.Read(&request_, &on_read_);
            }
        }
    }
    void onRead(bool ok) {
        if (asyncOperationFinished(RPCJob::AsyncOperationType::READ)) {
            if (ok) {
                job_handlers_.processRequestHandler(this, &request_);
                asyncOperationStarted(RPCJob::AsyncOperationType::READ);
                grpc_responder_.Read(&request_, &on_read_);
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
                    sendResponse();
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
    bool sendResponse(const ResponseType* response) {
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
    ServiceType* service_;
    grpc::ServerCompletionQueue* completion_queue_;
    grpc::ServerContext server_context_;
    RequestType request_;
    JobHandlers job_handlers_;
    typename JobHandlers::SendResponseHandler send_response_handler_;
    typename JobHandlers::GRPCResponder grpc_responder_;
    TagProcessor on_init_;
    TagProcessor on_read_;
    TagProcessor on_write_;
    TagProcessor on_finish_;
    TagProcessor on_done_;
    std::list<ResponseType> response_queue_;
    bool server_stream_done_;
    bool client_stream_done_;
};

#endif
