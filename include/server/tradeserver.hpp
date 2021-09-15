#ifndef TRADE_SERVER_HPP
#define TRADE_SERVER_HPP

#include <memory>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <list>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include <thread>
#include <google/protobuf/message.h>
#include <google/protobuf/arena.h>

#include "orderentrystreamconnection.hpp"
#include "logger.hpp"
#include "orderbookmanager.hpp"
#include "unaryrpcjob.hpp"
#include "orderentry.grpc.pb.h"
#include "jobhandlers.hpp"
#include "util.hpp"

using ServiceType = orderentry::OrderEntryService::AsyncService;
using OERequestType = orderentry::OrderEntryRequest;
using OEResponseType = orderentry::OrderEntryResponse;
using OrderRejection = orderentry::OrderEntryRejection::RejectionReason;

namespace server {
class TradeServer {
public:
    TradeServer(char* port, const std::string& filename);
    ~TradeServer();
private:
    void initStatics() { // to avoid static initialisation UB
        entry_order_responders_.find(nullptr);
        client_streams_.find(12);
    }
    void handleRemoteProcedureCalls();
    struct OrderEntryResponder {
        std::function<bool(OEResponseType*)> send_resp;
        grpc::ServerContext* server_context;
    };
    void createOrderEntryRPC();
    void makeOrderEntryRPC();
    static void setOrderEntryContext(
        ServiceType* service, 
        OrderEntryStreamConnection* job, 
        grpc::ServerContext* serv_context, 
        std::function<bool(OEResponseType*)> response
    );
    static void orderEntryDone(ServiceType* service, OrderEntryStreamConnection* job, bool);
    static void orderEntryProcessor(OrderEntryStreamConnection* job, const OERequestType* order_entry);
    static void processNewOrder(
        OrderEntryStreamConnection* job, 
        const OERequestType* add_entry,
        const OrderEntryResponder* responder
    );
    static void processModifyOrder(
        OrderEntryStreamConnection* job, 
        const OERequestType* modify_entry,
        const OrderEntryResponder* responder
    );
    static void processCancelOrder(
        OrderEntryStreamConnection* job, 
        const OERequestType* cancel_entry,
        const OrderEntryResponder* responder
    );
    static void sendNewOrderAcknowledgement(
        const orderentry::NewOrder& new_order,
        const orderentry::OrderCommon& new_order_common,
        const OrderEntryResponder* responder
    );
    static void sendModifyOrderAcknowledgement(
        const orderentry::ModifyOrder& modify_order,
        const orderentry::OrderCommon& modify_order_common,
        const OrderEntryResponder* responder
    );
    static void sendCancelOrderAcknowledgement(
        const orderentry::CancelOrder& cancel_order,
        const OrderEntryResponder* responder
    );
    static bool userIDUsageRejection(
        OrderEntryStreamConnection* connection,
        const orderentry::OrderCommon& common
    );
    static void sendWrongUserIDRejection(
        const OrderEntryResponder* responder,
        const uint64_t correct_id,
        const orderentry::OrderCommon& order_common
    );
    static void processOrderResult(
        info::OrderResult order_result, 
        const OrderEntryResponder* responder
    );
    static const OrderRejection getRejectionType(
        info::RejectionReason rejection
    );
    static void acknowledgeEntry(
        OrderEntryStreamConnection* connection, const OERequestType* oe
    );
    void makeMarketDataRPC();

    logging::Logger logger_;
    std::mutex taglist_mutex_;
    tradeorder::OrderBookManager ordermanager_;
    std::unique_ptr<grpc::Server> trade_server_;
    std::unique_ptr<grpc::ServerCompletionQueue> cq_;
    std::vector<std::thread> threadpool_;
    orderentry::OrderEntryService::AsyncService order_entry_service_;
    std::unordered_map<RPCJob*, OrderEntryResponder> entry_order_responders_;
    using user_id = uint64_t;
    std::unordered_map<user_id, RPCJob*> client_streams_;
};
}

#endif
