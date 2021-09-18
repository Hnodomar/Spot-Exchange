#include "dataprovider.hpp"

using namespace dataprovider;

DataProvider::DataProvider(std::shared_ptr<grpc::Channel> channel) 
    : stub_(orderentry::MarketDataService::NewStub(channel))
    , socket_(io_context, udp::endpoint(udp::v4(), 9002))
{}

void DataProvider::initiateMarketDataStream() {
    acceptSubscriber();
    std::thread(io_context.run());
    std::unique_ptr<grpc::ClientReader<MDResponse>> market_data_reader(
        stub_->MarketData(&context_, MDRequest())
    );
    while (market_data_reader->Read(&market_data_)) {
        for (const auto& subscriber : subscribers_) {
            socket_.async_send_to(
                boost::asio::buffer(market_data_)
            );
        }
    }

}

std::string DataProvider::serialiseMarketData() {
    
}

void DataProvider::acceptSubscriber() {
    socket_.async_receive_from(
        boost::asio::buffer(temp_buffer_),
        temp_remote_endpoint_,
        [self(shared_from_this())](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                self->subscribers_.push_back(self->temp_remote_endpoint_);
                self->acceptSubscriber();
            }
        }
    );
}
