#ifndef TRADE_SERVER_HPP
#define TRADE_SERVER_HPP

#include <memory>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include <grpcpp/grpcpp.h>

#include "logger.hpp"
#include "serverconnection.hpp"
#include "ordermanager.hpp"

namespace server {
using tcp = boost::asio::ip::tcp;
class TradeServer {
public:
    TradeServer(char* port, const std::string& filename);
    ~TradeServer();
private:
    void handleRemoteProcedureCalls();
    void createOrderEntryRPC();
    logging::Logger logger_;
    tradeorder::OrderManager ordermanager_;
    std::unique_ptr<grpc::Server> trade_server_;
    std::unique_ptr<grpc::ServerCompletionQueue> cq_;
    orderentry::OrderEntry::AsyncService order_entry_service_;
};
}

#endif
