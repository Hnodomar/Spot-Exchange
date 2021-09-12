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

#include "logger.hpp"
#include "orderbookmanager.hpp"
#include "bidirstreamrpcjob.hpp"
#include "unaryrpcjob.hpp"
#include "orderentry.grpc.pb.h"
#include "jobhandlers.hpp"
#include "rpcprocessor.hpp"
#include "util.hpp"

using ServiceType = orderentry::OrderEntryService::AsyncService;
using OERequestType = orderentry::OrderEntryRequest;
using OEResponseType = orderentry::OrderEntryResponse;
using MsgFactory = google::protobuf::Arena;
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
        RPCJob* job, 
        grpc::ServerContext* serv_context, 
        std::function<bool(OEResponseType*)> response
    );
    static void orderEntryDone(ServiceType* service, RPCJob* job, bool);
    static void orderEntryProcessor(RPCJob* job, const OERequestType* order_entry);
    static void processNewOrder(
        RPCJob* job, 
        const OERequestType* add_entry,
        const OrderEntryResponder* responder
    );
    static void processModifyOrder(
        RPCJob* job, 
        const OERequestType* modify_entry,
        const OrderEntryResponder* responder
    );
    static void processCancelOrder(
        RPCJob* job, 
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
    static bool userIDTaken(
        const orderentry::OrderCommon& common,
        const uint64_t job_id,
        const OrderEntryResponder* responder
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

    void makeMarketDataRPC();
    inline static OEResponseType neworder_ack_; // re-use messages to avoid memory allocation overhead
    inline static OEResponseType modifyorder_ack_;
    inline static OEResponseType cancelorder_ack_;
    inline static OEResponseType orderfill_ack_;
    inline static OEResponseType rejection_ack_;

    logging::Logger logger_;
    std::mutex taglist_mutex_;
    inline static tradeorder::OrderBookManager ordermanager_;
    std::unique_ptr<grpc::Server> trade_server_;
    std::unique_ptr<grpc::ServerCompletionQueue> cq_;
    orderentry::OrderEntryService::AsyncService order_entry_service_;
    static MsgFactory arena_;
    std::list<RPC::CallbackTag> taglist_;
    RPC::RPCProcessor rpc_processor_;
    inline static std::unordered_map<RPCJob*, OrderEntryResponder> entry_order_responders_;
    using user_id = uint64_t;
    inline static std::unordered_map<user_id, RPCJob*> client_streams_;
};
}

#endif
