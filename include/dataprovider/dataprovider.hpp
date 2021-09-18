#ifndef DATA_PROVIDER_HPP
#define DATA_PROVIDER_HPP

#include <memory>
#include <boost/asio.hpp>
#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <thread>
#include <string>

#include "orderentry.grpc.pb.h"

namespace dataprovider {
using MDResponse = orderentry::MarketDataResponse;
using MDRequest = orderentry::InitiateMarketDataStreamRequest;
using udp = boost::asio::ip::udp;
class DataProvider : public std::enable_shared_from_this<DataProvider> {
public:
    DataProvider(std::shared_ptr<grpc::Channel> channel);
private:
    void initiateMarketDataStream();
    void acceptSubscriber();
    std::string serialiseMarketData();
    grpc::ClientContext context_;
    MDResponse market_data_;
    grpc::Status status_;
    std::unique_ptr<orderentry::MarketDataService::Stub> stub_;
    boost::asio::io_context io_context;
    udp::socket socket_;
    udp::endpoint temp_remote_endpoint_;
    std::array<char, 1> temp_buffer_;
    std::vector<udp::endpoint> subscribers_;
};
}

#endif
