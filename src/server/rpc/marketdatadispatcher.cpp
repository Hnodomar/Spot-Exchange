#include "marketdatadispatcher.hpp"

using namespace rpc;

MarketDataDispatcher::MarketDataDispatcher(grpc::ServerCompletionQueue* cq,
orderentry::MarketDataService::AsyncService* market_data_service)
    : market_data_service_(market_data_service)
    , market_data_writer_(&server_context_)
    , cq_(cq)
{
    write_marketdata_ = [this](bool success) {
        this->writeToMDPlatform(success);
    };
}

bool MarketDataDispatcher::initiateMarketDataDispatch() {
    market_data_service_->RequestMarketData(
        &server_context_,
        &md_request_,
        &market_data_writer_,
        cq_,
        cq_,
        this
    );
    void* tag;
    bool ok;
    std::cout << "Waiting for MDP grpc establishment..\n";
    GPR_ASSERT(cq_->Next(&tag, &ok));
    std::cout << "Connected!\n";
    return ok;
}

void MarketDataDispatcher::writeMarketData(const MDResponseType* marketdata) {
    std::lock_guard<std::mutex> lock(mdmutex_);
    std::cout << "sending market data" << std::endl;
    market_data_queue_.push_back(*marketdata);
    if (!write_in_progress_) {
        market_data_writer_.Write(*marketdata, &write_marketdata_);
        write_in_progress_ = true;
    }
}

void MarketDataDispatcher::writeToMDPlatform(bool success) {
    std::lock_guard<std::mutex> lock(mdmutex_);
    market_data_queue_.pop_front();
    if (!market_data_queue_.empty() && success) {
        market_data_writer_.Write(market_data_queue_.front(), &write_marketdata_);
    }
    else
        write_in_progress_ = false;
}
