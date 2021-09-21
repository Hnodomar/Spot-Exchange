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
#include <cstdlib>
#include <iostream>

#include "order.hpp"
#include "util.hpp"
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
    bool constructNewOrderRequest(OERequest& request, const std::string& input);
    bool constructModifyOrderRequest(OERequest& request, const std::string& input);
    bool constructCancelOrderRequest(OERequest& request, const std::string& input);
    std::string findField(const std::string& field, const std::string& input) const;
    std::vector<std::string> getOrderValues(const std::string& input, const std::vector<std::string>& fields);
    bool getUserInput(OERequest& request);
    template<typename OrderType>
    bool setSide(OrderType& ordertype, char side);

    grpc::ClientContext context_;
    boost::asio::io_context io_context_;
    grpc::CompletionQueue cq_;
    grpc::Status status_;
    std::unique_ptr<orderentry::OrderEntryService::Stub> stub_;
    std::unordered_map<order_id, tradeorder::Order> orders_;
    udp::endpoint local_endpoint_;
    udp::socket socket_;
    udp::resolver resolver_;
    udp::endpoint marketdata_platform_;
    std::vector<std::thread> threads_;
    std::vector<std::string> addorder_fields_ = {
        "-side ", "-price ", "-quantity ", "-ticker ",
        "-userid "
    };
    std::vector<std::string> modorder_fields_ = {
        "-side ", "-price ", "-quantity ", "-ticker ",
        "-userid ", "-orderid "
    };
    std::vector<std::string> cancelorder_fields_ = {
        "-ticker ", "-userid ", "-orderid "
    };
    char buffer_[255] = {0};
};
template<typename OrderType>
inline bool TradingClient::setSide(OrderType& ordertype, char side) {
    switch(side) {
        case 'B':
            ordertype->set_is_buy_side(1);
            return true;
        case 'S':
            ordertype->set_is_buy_side(0);
            return true;
        default:
            return false;
    }
}

}
#endif
