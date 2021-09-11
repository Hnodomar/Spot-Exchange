#include "tradeserver.hpp"

using namespace server;

TradeServer::TradeServer(char* port, const std::string& outputfile="") 
  : logger_(outputfile)
  , rpc_processor_(taglist_, taglist_mutex_) {
    initStatics();
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
    client_streams_.erase(job->getUserID());
    delete job;
}

void TradeServer::orderEntryProcessor(RPCJob* job, const OERequestType* order_entry) {
    if (order_entry == nullptr) {
        return; // error
    }
    auto order_type = order_entry->OrderEntryType_case();
    using type = OERequestType::OrderEntryTypeCase;
    using namespace ::tradeorder;
    using namespace server::matching;
    switch(order_type) {
        case type::kNewOrder: {
            auto responderitr = entry_order_responders_.find(job);
            if (responderitr == entry_order_responders_.end()) {
                throw EngineException(
                    "Unable to find entry order responder for order entry ID "
                    + order_entry->new_order().order_common().order_id()
                );
            }
            const auto& new_order = order_entry->new_order();
            const auto& order_common = new_order.order_common();
            if (order_common.user_id() != job->getUserID()) {
                sendWrongUserIDRejection(
                    &responderitr->second, 
                    job->getUserID(),
                    order_common
                );
                return;
            }
            const auto userid_itr = client_streams_.find(order_common.user_id());
            if (userid_itr == client_streams_.end()) {
                client_streams_.emplace(order_common.user_id(), job);
            }
            sendNewOrderAcknowledgement(new_order, order_common, &responderitr->second);
            processMatchResults(
                ordermanager_.addOrder(
                    Order(
                        new_order.is_buy_side(),
                        new_order.price(),
                        order_common.order_id(),
                        order_common.ticker(),
                        new_order.quantity(),
                        order_common.user_id()
                    )
                ),
                &responderitr->second
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

void TradeServer::sendWrongUserIDRejection(OrderEntryResponder* responder, uint64_t correct_id,
const orderentry::OrderCommon& order_common) {
    auto rejection = rejection_ack_.mutable_rejection();
    auto common = rejection->mutable_order_common();
    common->set_user_id(correct_id);
    common->set_ticker(order_common.ticker());
    common->set_order_id(order_common.order_id());
    rejection->set_rejection_response(orderentry::OrderEntryRejection::wrong_user_id);
    responder->send_resp(&rejection_ack_);
}

void TradeServer::processMatchResults(matching::MatchResult match_result,
OrderEntryResponder* responder) {
    for (const auto& fill : match_result.getFills()) {
        auto fill_ack = orderfill_ack_.mutable_fill();
        fill_ack->set_timestamp(fill.timestamp);
        fill_ack->set_fill_quantity(fill.fill_qty);
        fill_ack->set_complete_fill(fill.full_fill);
        // fill_ack->set_fill_id(fill.fill_id);
        auto itr = client_streams_.find(fill.user_id);
        if (itr == client_streams_.end()) {
            return; // error
        }
        entry_order_responders_[itr->second].send_resp(
            &orderfill_ack_
        );
    }
}

void TradeServer::sendNewOrderAcknowledgement(const orderentry::NewOrder& new_order, 
const orderentry::OrderCommon& new_order_common, OrderEntryResponder* responder) {
    auto noa_status = neworder_ack_.mutable_new_order_ack();
    noa_status->set_is_buy_side(new_order.is_buy_side());
    noa_status->set_quantity(new_order.quantity());
    noa_status->set_price(new_order.price());
    auto noa_com_status = noa_status->mutable_status_common();
    noa_com_status->set_order_id(new_order_common.order_id());
    noa_com_status->set_ticker(new_order_common.ticker());
    noa_com_status->set_user_id(new_order_common.user_id());
    noa_status->set_timestamp(util::getUnixTimestamp());
    responder->send_resp(&neworder_ack_);
}

void TradeServer::handleRemoteProcedureCalls() {
    makeOrderEntryRPC();
    std::thread rpcprocessing(std::ref(rpc_processor_));
    RPC::CallbackTag cb_tag;
    for (;;) { // main grpc tag loop
        GPR_ASSERT(cq_->Next((void**)&cb_tag.callback_fn, &cb_tag.ok));
        taglist_mutex_.lock();
        taglist_.push_back(cb_tag);
        taglist_mutex_.unlock();
    }
}

void TradeServer::createOrderEntryRPC() {
    
}
