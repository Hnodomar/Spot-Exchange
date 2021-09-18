#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <thread>
#include <iostream>

#include "order.hpp"
#include "orderentry.grpc.pb.h"

namespace client {

using order_id = uint64_t;
using OERequest = orderentry::OrderEntryRequest;
using OEResponse = orderentry::OrderEntryResponse;
using AckType = OEResponse::OrderStatusTypeCase;
using RespType = OEResponse::ResponseTypeCase;

class TradingClient {
public:
    TradingClient(std::shared_ptr<grpc::Channel> channel);
    void startOrderEntry();
private:
    void interpretResponseType(OEResponse& oe_response);
    void makeNewOrderRequest(OERequest& request);
    void getUserInput();
    grpc::ClientContext context_;
    grpc::CompletionQueue cq_;
    grpc::Status status_;
    std::unique_ptr<orderentry::OrderEntryService::Stub> stub_;
    std::unordered_map<order_id, tradeorder::Order> orders_;
};
}
#endif
