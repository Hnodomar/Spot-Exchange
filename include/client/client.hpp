#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <boost/asio.hpp>
#include <boost/function_output_iterator.hpp>
#include <boost/lambda/lambda.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <thread>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "order.hpp"
#include "util.hpp"
#include "orderentry.grpc.pb.h"
#include "marketdatatypes.hpp"
#include "clientfeedhandler.hpp"
#include "clientorderbook.hpp"
#include "centerformatting.hpp"

namespace client {
constexpr uint8_t HEADER_LEN = 2;
using udp = boost::asio::ip::udp;
using order_id = uint64_t;
using OERequest = orderentry::OrderEntryRequest;
using OEResponse = orderentry::OrderEntryResponse;
using NewOrderAck = orderentry::NewOrderStatus;
using ModOrderAck = orderentry::ModifyOrderStatus;
using CancelOrderAck = orderentry::CancelOrderStatus;
using FillAck = orderentry::OrderEntryFill;
using RejectAck = orderentry::OrderEntryRejection;
using AckType = OEResponse::OrderStatusTypeCase;
using RespType = OEResponse::ResponseTypeCase;
using RejectType = orderentry::OrderEntryRejection::RejectionReason;
using Common = orderentry::OrderCommon;

class TradingClient : std::enable_shared_from_this<TradingClient> {
public:
    TradingClient(std::shared_ptr<grpc::Channel> channel, 
        const char* dp_hostname, const char* dataplatform_port);
    void startOrderEntry();
private:
    void interpretResponseType(OEResponse& oe_response);
    void subscribeToDataPlatform(const char* hostname, const char* port);
    void readMarketData();
    void processMarketData();
    void processAddOrderData();
    void processModifyOrderData();
    void processCancelOrderData();
    void processFillOrderData();
    bool userEnteredCommand(const std::string& command);
    void processNotificationData();
    bool constructNewOrderRequest(OERequest& request, const std::string& input);
    bool constructModifyOrderRequest(OERequest& request, const std::string& input);
    bool constructCancelOrderRequest(OERequest& request, const std::string& input);
    std::string findField(const std::string& field, const std::string& input) const;
    std::vector<std::string> getOrderValues(const std::string& input, const std::vector<std::string>& fields);
    bool getUserInput(OERequest& request);
    template<typename OrderType>
    bool setSide(OrderType& ordertype, char side);
    void promptUserID();
    void interpretAck(const NewOrderAck& new_ack);
    void interpretAck(const ModOrderAck& mod_ack);
    void interpretAck(const CancelOrderAck& cancel_ack);
    void interpretAck(const FillAck& fill_ack);
    void interpretAck(const RejectAck& reject_ack);
    std::string rejectionToString(const RejectType rejection) const;
    std::vector<std::string> split(const std::string& s, char delimiter);
    std::string commonStr(int64_t timestamp, const Common& common);
    std::string commonStr(const Common& common);
    std::string timestampStr(int64_t timestamp) const;
    uint64_t getUserID() const {return userID_;}
    void printInfoBox(bool clear_prev = true);
    void reprintInterface();
    void removeUserInput() const {std::cout << "\033[A" << "\33[2K";}

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
    ClientFeedHandler feedhandler_;
    ClientOrderBook* subscription_ = nullptr;
    std::vector<std::thread> threads_;
    std::vector<std::string> info_feed_ = {
        " ", " ", " ", " ", " "
    };
    std::vector<std::string> addorder_fields_ = {
        "-side ", "-price ", "-quantity ", "-ticker "
    };
    std::vector<std::string> modorder_fields_ = {
        "-side ", "-price ", "-quantity ", "-ticker ",
        "-orderid "
    };
    std::vector<std::string> cancelorder_fields_ = {
        "-ticker ", "-orderid "
    };
    uint64_t userID_ = 0;
    char buffer_[64] = {0};
    uint8_t data_length_ = 0;
    uint8_t packet_type_ = 0;
    uint8_t prev_height_ = 0;
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
