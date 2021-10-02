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
    logging::Logger::Log(
        logging::LogType::Debug,
        util::getLogTimestamp(),
        "Blocking: waiting for market data dispatcher connection to initiate"
    );
    GPR_ASSERT(cq_->Next(&tag, &ok));
    logging::Logger::Log(
        logging::LogType::Debug,
        util::getLogTimestamp(),
        "Market data dispatcher connection successful"
    );
    return ok;
}

void MarketDataDispatcher::writeMarketData(const MDResponseType* marketdata) {
    if (marketdata->add().order_id() == 356198) {
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> time = t1 - OrderEntryStreamConnection::t0;
        std::cout << "BENCHMARK COMPLETE: " << std::endl;
        std::cout << "120000 orders in " << (time / 1000.0).count() << " seconds" << std::endl;
        std::cout << 120000.0 / (time / 1000.0).count() << " orders a second" << std::endl;
        exit(0);
    }
    std::lock_guard<std::mutex> lock(mdmutex_);
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
