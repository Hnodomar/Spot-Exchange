#include "tradeserver.hpp"

using namespace server;

TradeServer::TradeServer(char* port, const std::string& outputfile="") 
: logger_(outputfile), rpc_processor_(taglist_, taglist_mutex_, condv_) {
    std::string server_address("0.0.0.0:" + std::string(port));
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&order_entry_service_);
    cq_ = builder.AddCompletionQueue();
    trade_server_ = builder.BuildAndStart();
    logger_.write("Server listening on " + server_address);
}

void TradeServer::makeMarketDataRPC() {

}

void TradeServer::makeOrderEntryRPC() {
    BiDirectionalStreamHandler<ServiceType, OERequestType, OEResponseType> job_handlers;
    job_handlers.rpcJobContextHandler = &setOrderEntryContext;
    job_handlers.rpcJobDoneHandler = &orderEntryDone;
    job_handlers.createRPCJobHandler = std::bind(&TradeServer::makeOrderEntryRPC, this);
    job_handlers.queueReqHandler = &orderentry::OrderEntryService::AsyncService::RequestOrderEntry;
    job_handlers.processRequestHandler = &orderEntryProcessor;
    new BiDirStreamRPCJob<ServiceType, OERequestType, OEResponseType>(&order_entry_service_, cq_.get(), job_handlers);
}

void TradeServer::setOrderEntryContext(ServiceType* service, RPCJob* job, 
grpc::ServerContext* serv_context, std::function<bool(OEResponseType*)> send_response) {
    OrderEntryResponder responder;
    responder.send_resp = send_response;
    responder.server_context = serv_context;
    entry_order_responders_.emplace(job, responder);
}

void TradeServer::orderEntryDone(ServiceType* service, RPCJob* job, bool) {
    entry_order_responders_.erase(job);
    delete job;
}

void TradeServer::orderEntryProcessor(RPCJob* job, const OERequestType* order_entry) {
    auto order_type = order_entry->OrderEntryType_case();
    using type = OERequestType::OrderEntryTypeCase;
    using namespace ::tradeorder;
    switch(order_type) {
        case type::kNewOrder: {
            auto new_order = order_entry->new_order();
            auto order_common = new_order.order_common();
            ordermanager_.addOrder(
                Order(
                    new_order.is_buy_side(),
                    static_cast<uint64_t>(new_order.price()),
                    static_cast<uint64_t>(order_common.order_id()),
                    static_cast<uint64_t>(order_common.ticker()),
                    static_cast<uint32_t>(new_order.quantity()),
                    order_common.username()
                )
            );
            break;
        }
        case type::kModifyOrder: {
            auto modify_order = order_entry->modify_order();
            auto order_common = modify_order.order_common();
            break;
        }
        case type::kCancelOrder: {
            auto cancel_order = order_entry->cancel_order();
            auto order_common = cancel_order.order_common();
            break;
        }
        default:
            // log
            break;
    }
}

void TradeServer::handleRemoteProcedureCalls() {
    makeOrderEntryRPC();
    std::thread rpcprocessing(&rpc_processor_);
    RPC::CallbackTag cb_tag;
    for (;;) { // main grpc tag loop
        GPR_ASSERT(cq_->Next((void**)&cb_tag.callback_fn, &cb_tag.ok));
        taglist_mutex_.lock();
        taglist_.push_back(cb_tag);
        taglist_mutex_.unlock();
        condv_.notify_one();
    }
}

void TradeServer::createOrderEntryRPC() {
    
}
