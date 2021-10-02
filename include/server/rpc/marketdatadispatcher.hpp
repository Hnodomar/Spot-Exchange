#ifndef MARKETDATA_DISPATCHER_HPP
#define MARKETDATA_DISPATCHER_HPP

#include <memory>
#include <thread>
#include <list>
#include <chrono>
#include <mutex>

#include "orderentrystreamconnection.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "orderentry.grpc.pb.h"

using MDResponseType = orderentry::MarketDataResponse;
using MDRequestType = orderentry::InitiateMarketDataStreamRequest;
namespace rpc {
using WriteCallback = std::function<void(bool)>;
using Time = std::chrono::_V2::system_clock::time_point;
class MarketDataDispatcher {
public:
    MarketDataDispatcher(
        grpc::ServerCompletionQueue* cq,
        orderentry::MarketDataService::AsyncService* market_data_service
    );
    MarketDataDispatcher(const MarketDataDispatcher&) = default;
    MarketDataDispatcher& operator=(MarketDataDispatcher& rhs) = default;
    bool initiateMarketDataDispatch();
    void writeMarketData(const MDResponseType* marketdata);
    void setCQ(grpc::ServerCompletionQueue* cq) {cq_ = cq;}
private:
    void writeToMDPlatform(bool success);
    orderentry::MarketDataService::AsyncService* market_data_service_;
    grpc::ServerAsyncWriter<MDResponseType> market_data_writer_;
    grpc::ServerCompletionQueue* cq_;
    grpc::ServerContext server_context_;
    std::list<MDResponseType> market_data_queue_;
    WriteCallback write_marketdata_;
    MDResponseType md_response_;
    MDRequestType md_request_;
    std::mutex mdmutex_;
    bool write_in_progress_ = false;
};
}

#endif
