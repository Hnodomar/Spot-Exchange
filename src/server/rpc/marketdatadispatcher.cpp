#include "marketdatadispatcher.hpp"

using namespace rpc;

MarketDataDispatcher::MarketDataDispatcher(grpc::ServerCompletionQueue* cq,
orderentry::MarketDataService::AsyncService* market_data_service)
    : market_data_service_(market_data_service)
    , cq_(cq)
    , market_data_writer_(&server_context_)
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
    GPR_ASSERT(cq_->Next(&tag, &ok));
    GPR_ASSERT(ok);
    return ok;
}

void MarketDataDispatcher::writeMarketData(const MDResponseType* marketdata) {
    std::lock_guard<std::mutex> lock(mdmutex_);
    market_data_queue_.push_back(*marketdata);
    alarm_.Set(
        cq_, 
        gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME), 
        &write_marketdata_
    );
}

void MarketDataDispatcher::writeToMDPlatform(bool success) {
    if (!market_data_queue_.empty() && success) {
        std::lock_guard<std::mutex> lock(mdmutex_);
        market_data_writer_.Write(market_data_queue_.front(), &write_marketdata_);
        market_data_queue_.pop_front();
    }
}
