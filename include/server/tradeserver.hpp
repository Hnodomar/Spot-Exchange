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

#include "orderentry.grpc.pb.h"
#include "orderbookmanager.hpp"
#include "logger.hpp"
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
    void handleRemoteProcedureCalls();
    void createOrderEntryRPC();
    void makeOrderEntryRPC();
    void makeMarketDataRPC();

    logging::Logger logger_;
    std::mutex taglist_mutex_;
    tradeorder::OrderBookManager ordermanager_;
    std::unique_ptr<grpc::Server> trade_server_;
    std::unique_ptr<grpc::ServerCompletionQueue> cq_;
    std::vector<std::thread> threadpool_;
    orderentry::OrderEntryService::AsyncService order_entry_service_;
    using user_id = uint64_t;
    std::unordered_map<user_id, OrderEntryStreamConnection*> client_streams_;
};
}

#endif
