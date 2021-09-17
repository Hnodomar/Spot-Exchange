#ifndef TRADE_SERVER_HPP
#define TRADE_SERVER_HPP

#include <memory>
#include <fstream>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <list>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
#include <thread>

#include "orderentryjobhandlers.hpp"

#include "orderentry.grpc.pb.h"
#include "orderentrystreamconnection.hpp"
#include "orderbookmanager.hpp"
#include "order.hpp"
#include "level.hpp"
#include "fifomatching.hpp"
#include "logger.hpp"
#include "util.hpp"

using ServiceType = orderentry::OrderEntryService::AsyncService;
using OERequestType = orderentry::OrderEntryRequest;
using OEResponseType = orderentry::OrderEntryResponse;
using OrderRejection = orderentry::OrderEntryRejection::RejectionReason;
using user_id = uint64_t;

namespace server {
class TradeServer {
public:
    TradeServer(char* port, const std::string& filename);
    ~TradeServer();
private:
    void handleRemoteProcedureCalls();
    void createOrderEntryRPC();
    void makeOrderEntryRPC();
    void makeMarketDataRPC();
    void setConnectionContext();
    static void makeNewOrderEntryConnection();

    logging::Logger logger_;
    std::mutex taglist_mutex_;
    tradeorder::OrderBookManager ordermanager_;
    std::unique_ptr<grpc::Server> trade_server_;
    static std::unique_ptr<grpc::ServerCompletionQueue> cq_;
    std::vector<std::thread> threadpool_;
    static orderentry::OrderEntryService::AsyncService order_entry_service_;
    static std::unordered_map<user_id, OrderEntryStreamConnection*> client_streams_;
    static OEJobHandlers job_handlers_;
};
}

#endif
