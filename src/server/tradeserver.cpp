#include "tradeserver.hpp"

using namespace server;

orderentry::OrderEntryService::AsyncService TradeServer::order_entry_service_;
orderentry::MarketDataService::AsyncService TradeServer::market_data_service_;
std::unordered_map<user_id, OrderEntryStreamConnection*> TradeServer::client_streams_;
std::unique_ptr<grpc::ServerCompletionQueue> TradeServer::cq_;
std::unique_ptr<grpc::Server> TradeServer::trade_server_;

void sigintHandler(int sig_no) {
    TradeServer::shutdownServer();
}

TradeServer::TradeServer(char* port, const std::string& outputfile="") 
  : marketdata_dispatcher_(nullptr, &market_data_service_)
  , ordermanager_(&marketdata_dispatcher_)
{
    logger_.setOutputFile(outputfile);
    std::string server_address("127.0.0.1:" + std::string(port));
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&order_entry_service_);
    builder.RegisterService(&market_data_service_);
    cq_ = builder.AddCompletionQueue();
    marketdata_dispatcher_.setCQ(cq_.get());
    trade_server_ = builder.BuildAndStart();
    logging::Logger::Log(logging::LogType::Info, util::getLogTimestamp(), "Server listening on " + server_address);
    disposition_.sa_handler = &sigintHandler;
    sigemptyset(&disposition_.sa_mask);
    disposition_.sa_flags = 0;
    sigaction(SIGINT, &disposition_, NULL);
    ordermanager_.createOrderBook("AAPL");
    handleRemoteProcedureCalls();
}

void TradeServer::shutdownServer() {
    for (const auto& connection : client_streams_)
        connection.second->onStreamCancelled(true);
    trade_server_.get()->Shutdown();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    cq_.get()->Shutdown();
    std::cout << "Shutdown.\n";
    exit(0);
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
    if (!marketdata_dispatcher_.initiateMarketDataDispatch()) {
        std::cout << "failed to connect to market data platform successfully\n";
        exit(1);
        // error
    }
    makeNewOrderEntryConnection();
    for (uint i = 0; i < std::thread::hardware_concurrency(); ++i) {
        threadpool_.emplace_back(std::thread(rpcprocessor));
    }
    while(true) {

    }
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
