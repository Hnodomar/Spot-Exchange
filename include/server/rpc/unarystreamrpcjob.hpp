#ifndef UNARY_STREAM_RPC_JOB_HPP
#define UNARY_STREAM_RPC_JOB_HPP

#include <list>
#include "jobhandlers.hpp"
#include "rpcjob.hpp"

template<typename ServiceType, typename RequestType, typename ResponseType>
class UnaryRPCJob : public RPCJob {
    using JobHandlers = UnaryHandler<ServiceType, RequestType, ResponseType>;
    using TagProcessor = std::function<void(bool)>;
public:
    UnaryRPCJob(ServiceType* service, grpc::ServerCompletionQueue* completion_q, JobHandlers job_handlers):
        service_(service),
        completion_queue_(completion_q),
        grpc_responder_(&server_context_),
        job_handlers_(job_handlers) {
        on_read_ = std::bind(&UnaryRPCJob::onRead, this, std::placeholders::_1);
        on_finish_ = std::bind(&UnaryRPCJob::onFinish, this, std::placeholders::_1);
        on_done_ = std::bind(&RPCJob::onDone, this, std::placeholders::_1);
        server_context_.AsyncNotifyWhenDone(&on_done_);
        send_response_handler_ = std::bind(&UnaryRPCJob::sendResponse, this, std::placeholders::_1);
        job_handlers_.rpcJobContextHandler(service_, this, &server_context_, send_response_handler_);
        asyncOperationStarted(RPCJob::AsyncOperationType::QUEUED_REQUEST);
        job_handlers_.queueReqHandler(
            service_, 
            &server_context_, 
            &request_, 
            &grpc_responder_, 
            completion_queue_, 
            completion_queue_, 
            &on_read_
        );
    }
private:
    bool sendResponse(const ResponseType* response) {
        GPR_ASSERT(response);
        if (response == nullptr)
            return false;
        response_ = *response;
        asyncOperationStarted(RPCJob::AsyncOperationType::FINISH);
        grpc_responder_.Finish(response_, grpc::STATUS::OK, &on_finish_);
        return true;
    }
    void onRead(bool ok) {
        job_handlers_.createRPCJobHandler();
        if (asyncOperationFinished(RPCJob::AsyncOperationType::QUEUED_REQUEST)) {
            if (ok) {
                job_handlers_.processRequestHandler(service_, this, &request_);
            }
            else
                GPR_ASSERT(ok);
        }
    }
    void onFinish(bool ok) {
        asyncOperationFinished(RPCJob::AsyncOperationType::FINISH);
    }
    void done() override {
        job_handlers_.rpcJobDoneHandler(service_, this, server_context_.IsCancelled());
    }
    ServiceType* service_;
    grpc::ServerCompletionQueue* completion_queue_;
    typename JobHandlers::GRPCResponder grpc_responder_;
    typename JobHandlers::SendResponseHandler send_response_handler_;
    grpc::ServerContext server_context_;
    RequestType request_;
    ResponseType response_;
    JobHandlers job_handlers_;
    TagProcessor on_read_;
    TagProcessor on_finish_;
    TagProcessor on_done_;
};

#endif
