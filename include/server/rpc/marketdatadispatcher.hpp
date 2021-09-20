#ifndef MARKETDATA_DISPATCHER_HPP
#define MARKETDATA_DISPATCHER_HPP

#include <memory>
#include <thread>
#include <list>
#include <mutex>

#include "orderentry.grpc.pb.h"

using MDResponseType = orderentry::MarketDataResponse;
using MDRequestType = orderentry::InitiateMarketDataStreamRequest;
namespace rpc {
using WriteCallback = std::function<void(bool)>;
class MarketDataDispatcher {
public:
    MarketDataDispatcher(
        grpc::ServerCompletionQueue* cq,
        orderentry::MarketDataService::AsyncService* market_data_service
    );
    bool initiateMarketDataDispatch();
    void writeMarketData(const MDResponseType* marketdata);
private:
    void marketDataPushBack(const MDResponseType* marketdata);
    void marketDataWrite(const MDResponseType* marketdata);
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
    void (rpc::MarketDataDispatcher::*write_fn_)(const MDResponseType*);
};
}

#endif
