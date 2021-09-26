#ifndef DATA_PROVIDER_HPP
#define DATA_PROVIDER_HPP

#include <memory>
#include <boost/asio.hpp>
#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <thread>
#include <string>
#include <algorithm>

#include "orderentry.grpc.pb.h"

namespace dataplatform {
using MDResponse = orderentry::MarketDataResponse;
using MDRequest = orderentry::InitiateMarketDataStreamRequest;
using type = orderentry::MarketDataResponse::OrderEntryTypeCase;
using udp = boost::asio::ip::udp;
constexpr uint8_t add_data_len_ = 37;
constexpr uint8_t mod_data_len_ = 20;
constexpr uint8_t cancel_data_len_ = 16;
constexpr uint8_t fill_data_len_ = 46;
constexpr uint8_t notification_len_ = 1;
class DataPlatform : public std::enable_shared_from_this<DataPlatform> {
public:
    DataPlatform(std::shared_ptr<grpc::Channel> channel);
    void initiateMarketDataStream();
private:
    void acceptSubscriber();
    void serialiseMarketData();
    template<typename Data>
    void serialiseBytes(char*& ptr, Data timestamp);     
    void serialiseAddOrder();
    void serialiseModOrder();
    void serialiseCancel();
    void serialiseNotification();
    void serialiseFill();
    grpc::ClientContext context_;
    MDResponse market_data_;
    grpc::Status status_;
    std::unique_ptr<orderentry::MarketDataService::Stub> stub_;
    boost::asio::io_context io_context;
    udp::socket socket_;
    udp::endpoint temp_remote_endpoint_;
    std::array<char, 255> temp_buffer_;
    std::array<char, 1> conn_buffer_;
    std::set<udp::endpoint> subscribers_;
};
template<typename Data>
void DataPlatform::serialiseBytes(char*& ptr, Data data) {            
    std::memcpy(ptr, &data, sizeof(data));
    ptr += sizeof(data);

}
}

#endif
