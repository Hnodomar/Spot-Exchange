#include "tradeserver.hpp"

using namespace server;

orderentry::OrderEntryService::AsyncService TradeServer::order_entry_service_;
std::unordered_map<user_id, OrderEntryStreamConnection*> TradeServer::client_streams_;
std::unique_ptr<grpc::ServerCompletionQueue> TradeServer::cq_;

TradeServer::TradeServer(char* port, const std::string& outputfile="") 
  : logger_(outputfile) {
    std::string server_address("0.0.0.0:" + std::string(port));
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&order_entry_service_);
    cq_ = builder.AddCompletionQueue();
    trade_server_ = builder.BuildAndStart();
    logger_.write("Server listening on " + server_address);
    std::cout << "Hello World!\n";
}

void TradeServer::handleRemoteProcedureCalls() {
    auto rpcprocessor = [this](){
        std::function<void(bool)>* callback;
        bool ok;
        for (;;) {
            GPR_ASSERT(cq_->Next((void**)&callback, &ok));
            (*(callback))(ok);
        }
    };
    makeNewOrderEntryConnection();
    for (uint i = 0; i < std::thread::hardware_concurrency(); ++i) {
        threadpool_.emplace_back(std::thread(rpcprocessor));
    }
}

void TradeServer::setConnectionContext() {
    
}

OEJobHandlers TradeServer::job_handlers_ = {
    &server::tradeorder::OrderBookManager::addOrder,
    &server::tradeorder::OrderBookManager::modifyOrder,
    &server::tradeorder::OrderBookManager::cancelOrder,
    &makeNewOrderEntryConnection
};

void TradeServer::makeNewOrderEntryConnection() {
    new OrderEntryStreamConnection(
        &order_entry_service_, cq_.get(), client_streams_, job_handlers_
    );
}
