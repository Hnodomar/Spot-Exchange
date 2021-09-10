#ifndef JOB_HANDLERS_HPP
#define JOB_HANDLERS_HPP

#include <functional>
#include <grpc/grpc.h>
#include <grpc++/server_context.h>
#include <memory>

#include "rpcjob.hpp"

template<typename ServiceType, typename RequestType, typename ResponseType>
struct JobHandlers {
    using ProcessRequestHandler = std::function<void(RPCJob*, const RequestType*)>;
    using CreateRPCJobHandler = std::function<void()>;
    using RPCJobDoneHandler = std::function<void(ServiceType*, RPCJob*, bool)>;
    using SendResponseHandler = std::function<bool(const ResponseType*)>;
    using RPCJobContextHandler = std::function<void(ServiceType*, RPCJob*, grpc::ServerContext*, SendResponseHandler)>;
    ProcessRequestHandler processRequestHandler;
    CreateRPCJobHandler createRPCJobHandler;
    RPCJobDoneHandler rpcJobDoneHandler;
    RPCJobContextHandler rpcJobContextHandler;
};

template<typename ServiceType, typename RequestType, typename ResponseType>
struct BiDirectionalStreamHandler : public JobHandlers<ServiceType, RequestType, ResponseType> {
    using GRPCResponder = grpc::ServerAsyncReaderWriter<ResponseType, RequestType>;
    using QueueRequestHandler = std::function<void(
        ServiceType*, grpc::ServerContext*, GRPCResponder*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*
    )>;
    QueueRequestHandler queueReqHandler;
};

template<typename ServiceType, typename RequestType, typename ResponseType>
struct UnaryHandler : public JobHandlers<ServiceType, RequestType, ResponseType> {
    using GRPCResponder = grpc::ServerAsyncResponseWriter<ResponseType>;
    using QueueRequestHandler = std::function<void(
        ServiceType*, grpc::ServerContext*, RequestType*, GRPCResponder*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*
    )>;
    QueueRequestHandler queueReqHandler;
};

#endif
