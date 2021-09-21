#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <boost/asio.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <thread>
#include <iostream>

#include "order.hpp"
#include "orderentry.grpc.pb.h"

namespace client {

using udp = boost::asio::ip::udp;
using order_id = uint64_t;
using OERequest = orderentry::OrderEntryRequest;
using OEResponse = orderentry::OrderEntryResponse;
using AckType = OEResponse::OrderStatusTypeCase;
using RespType = OEResponse::ResponseTypeCase;

class TradingClient : std::enable_shared_from_this<TradingClient> {
public:
    TradingClient(std::shared_ptr<grpc::Channel> channel, 
        const char* dp_hostname, const char* dataplatform_port);
    void startOrderEntry();
private:
    void interpretResponseType(OEResponse& oe_response);
    void makeNewOrderRequest(OERequest& request);
    void subscribeToDataPlatform(const char* hostname, const char* port);
    void readMarketData();
    bool getUserInput();
    grpc::ClientContext context_;
    grpc::CompletionQueue cq_;
    grpc::Status status_;
    std::unique_ptr<orderentry::OrderEntryService::Stub> stub_;
    std::unordered_map<order_id, tradeorder::Order> orders_;
    udp::socket socket_;
    udp::resolver resolver_;
    udp::endpoint marketdata_platform_;
    boost::asio::io_context io_context_;
    char buffer_[255] = {0};
};
}
#endif
