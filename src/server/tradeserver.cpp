#include "tradeserver.hpp"

using namespace server;

TradeServer::TradeServer(char* port, const std::string& outputfile="") 
: logger_(outputfile) {
    std::string server_address("0.0.0.0:" + std::string(port));
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&order_entry_service_);
    cq_ = builder.AddCompletionQueue();
    trade_server_ = builder.BuildAndStart();
    logger_.write("Server listening on " + server_address);
}

void TradeServer::handleRemoteProcedureCalls() {

}

void TradeServer::createOrderEntryRPC() {
    
}
