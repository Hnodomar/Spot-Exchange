#include "tradeserver.hpp"

using namespace server;

TradeServer::TradeServer(char* port, const std::string& outputfile="") 
  : logger_(outputfile) {
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
    new OrderEntryStreamConnection(
        &order_entry_service_, cq_.get(), job_handlers, client_streams_
    );
}

void TradeServer::setOrderEntryContext(ServiceType* service, OrderEntryStreamConnection* connection, 
grpc::ServerContext* serv_context, std::function<bool(OEResponseType*)> send_response) {
    OrderEntryResponder responder;
    responder.send_resp = send_response;
    responder.server_context = serv_context;
    entry_order_responders_.emplace(job, responder);
}
void TradeServer::orderEntryDone(ServiceType* service, OrderEntryStreamConnection* connection, bool) {
    entry_order_responders_.erase(connection);
    client_streams_.erase(connection->getUserID());
    delete connection;
}


bool TradeServer::userIDUsageRejection(OrderEntryStreamConnection* connection, const orderentry::OrderCommon& common) {
    if (common.user_id() != connection->getUserID()) {
        auto rejection = rejection_ack.mutable_rejection();
        auto rejection_common = rejection->mutable_order_common();
        rejection_common->set_user_id(common.user_id());
        rejection_common->set_ticker(common.ticker());
        rejection_common->set_order_id(common.order_id());
        rejection->set_rejection_response(orderentry::OrderEntryRejection::wrong_user_id);
        connection->writeResponse(&rejection_ack, );
        return false;
    }
    
    return true;
}

void TradeServer::sendWrongUserIDRejection(const OrderEntryResponder* responder, const uint64_t correct_id,
const orderentry::OrderCommon& order_common) {
    auto rejection = rejection_ack.mutable_rejection();
    auto common = rejection->mutable_order_common();
    common->set_user_id(correct_id);
    common->set_ticker(order_common.ticker());
    common->set_order_id(order_common.order_id());
    rejection->set_rejection_response(orderentry::OrderEntryRejection::wrong_user_id);
    responder->send_resp(&rejection_ack);
}

void TradeServer::orderEntryProcessor(OrderEntryStreamConnection* connection, const OERequestType* order_entry) {
    using type = OERequestType::OrderEntryTypeCase;
    using namespace ::tradeorder;
    using namespace server::matching;
    if (order_entry == nullptr) {
        return; // error
    }
    auto order_type = order_entry->OrderEntryType_case();
    auto responderitr = entry_order_responders_.find(job);
    if (responderitr == entry_order_responders_.end()) {
        throw EngineException(
            "Unable to find entry order responder for order entry ID "
            + order_entry->new_order().order_common().order_id()
        );
    }
    switch(order_type) {
        case type::kNewOrder: 
            processNewOrder(job, order_entry, &responderitr->second);
            break;
        case type::kModifyOrder:
            processModifyOrder(job, order_entry, &responderitr->second);
            break;
        case type::kCancelOrder:
            processCancelOrder(job, order_entry, &responderitr->second);
            break;
        default:
            // log
            break;
    }
}

void TradeServer::processNewOrder(OrderEntryStreamConnection* connection, const OERequestType* order_entry,
const OrderEntryResponder* responder) {
    using namespace tradeorder;
    using namespace server::matching;
    const auto& new_order = order_entry->new_order();
    const auto& order_common = new_order.order_common();
    if (userIDTaken(order_common, connection->getUserID(), responder)) {
        return;
    }
    const auto userid_itr = client_streams_.find(order_common.user_id());
    if (userid_itr == client_streams_.end()) {
        client_streams_.emplace(order_common.user_id(), job);
    }
    sendNewOrderAcknowledgement(new_order, order_common, responder);
    Order order(
        new_order.is_buy_side(),
        new_order.price(),
        new_order.quantity(),
        info::OrderCommon(
            order_common.order_id(),
            order_common.user_id(),
            order_common.ticker()
        )
    );
    processOrderResult(ordermanager_.addOrder(order), responder);
}

void TradeServer::processModifyOrder(OrderEntryStreamConnection* connection, const OERequestType* modify_entry,
const OrderEntryResponder* responder) {
    using namespace info;
    auto modify_order = modify_entry->modify_order();
    auto order_common = modify_order.order_common();
    if (userIDTaken(order_common, connection->getUserID(), responder)) {
        return;
    }
    auto modify_result = ordermanager_.modifyOrder(
        ModifyOrder(
            modify_order.quantity(),
            modify_order.is_buy_side(),
            modify_order.price(),
            info::OrderCommon(
                order_common.order_id(),
                order_common.user_id(),
                order_common.ticker()
            )
        )
    );
    processOrderResult(modify_result.first, responder);
    if (!modify_result.second.empty())
        processOrderResult(modify_result.second, responder);
}

void TradeServer::processCancelOrder(OrderEntryStreamConnection* connection, const OERequestType* cancel_entry,
const OrderEntryResponder* responder) {
    using namespace info;
    auto cancel_order = cancel_entry->cancel_order();
    auto order_common = cancel_order.order_common();
    if (userIDTaken(order_common, connection->getUserID(), responder)) {
        return;
    }
    processOrderResult(
        ordermanager_.cancelOrder(
            CancelOrder(
                order_common.order_id(),
                order_common.user_id(),
                order_common.ticker()
            )
        ),
        responder
    );
}

void TradeServer::processOrderResult(info::OrderResult order_result, const OrderEntryResponder* responder) {
    auto order_status = order_result.order_status_present;
    using StatusPresent = info::OrderStatusPresent;
    switch(order_status) {
        case StatusPresent::RejectionPresent: {
            auto rejection = rejection_ack.mutable_rejection();
            rejection->set_rejection_response(getRejectionType(order_result.orderstatus.rejection));
            responder->send_resp(&rejection_ack);
            return;
        }
        case StatusPresent::NewOrderPresent:
        case StatusPresent::ModifyOrderPresent: 
        case StatusPresent::CancelOrderPresent: 
        default:
            break;
    }

    if (order_result.match_result == std::nullopt)
        return;

    for (const auto& fill : order_result.match_result->getFills()) {
        auto fill_ack = orderfill_ack.mutable_fill();
        fill_ack->set_timestamp(fill.timestamp);
        fill_ack->set_fill_quantity(fill.fill_qty);
        fill_ack->set_complete_fill(fill.full_fill);
        // fill_ack->set_fill_id(fill.fill_id);
        auto itr = client_streams_.find(fill.user_id);
        if (itr == client_streams_.end()) {
            return; // error
        }
        entry_order_responders_[itr->second].send_resp(
            &orderfill_ack
        );
    }
}

const OrderRejection TradeServer::getRejectionType(info::RejectionReason rejection) {
    using reject = info::RejectionReason;
    using oereject = orderentry::OrderEntryRejection;
    switch(rejection) {
        case reject::unknown:
            return oereject::unknown;
        case reject::order_not_found:
            return oereject::order_not_found;
        case reject::order_id_already_present:
            return oereject::order_id_already_present;
        case reject::orderbook_not_found:
            return oereject::orderbook_not_found;
        case reject::ticker_not_found:
            return oereject::ticker_not_found;
        case reject::modify_wrong_side:
            return oereject::modify_wrong_side;
        case reject::modification_trivial:
            return oereject::modification_trivial;
        case reject::wrong_user_id:
            return oereject::wrong_user_id;
        default:
            return oereject::unknown;
    }
}

void TradeServer::sendNewOrderAcknowledgement(const orderentry::NewOrder& new_order, 
const orderentry::OrderCommon& new_order_common, const OrderEntryResponder* responder) {
    auto noa_status = neworder_ack.mutable_new_order_ack();
    noa_status->set_is_buy_side(new_order.is_buy_side());
    noa_status->set_quantity(new_order.quantity());
    noa_status->set_price(new_order.price());
    auto noa_com_status = noa_status->mutable_status_common();
    noa_com_status->set_order_id(new_order_common.order_id());
    noa_com_status->set_ticker(new_order_common.ticker());
    noa_com_status->set_user_id(new_order_common.user_id());
    noa_status->set_timestamp(util::getUnixTimestamp());
    responder->send_resp(&neworder_ack);
}

void TradeServer::handleRemoteProcedureCalls() {
    makeOrderEntryRPC();
    auto rpcprocessor = [this](){
        std::function<void(bool)>* callback;
        bool ok;
        for (;;) {
            GPR_ASSERT(cq_->Next((void**)&callback, &ok));
            (*(callback))(ok);
        }
    };
    for (uint i = 0; i < std::thread::hardware_concurrency(); ++i) {
        threadpool_.emplace_back(std::thread(rpcprocessor));
    }
}

void TradeServer::createOrderEntryRPC() {
    
}
