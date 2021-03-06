#include "dataplatform.hpp"

using namespace dataplatform;

DataPlatform::DataPlatform(std::shared_ptr<grpc::Channel> channel) 
    : stub_(orderentry::MarketDataService::NewStub(channel))
    , socket_(io_context, udp::endpoint(udp::v4(), 9002))
{}

void DataPlatform::initiateMarketDataStream() {
    acceptSubscriber();
    std::thread acceptloop([this](){io_context.run();});
    std::unique_ptr<grpc::ClientReader<MDResponse>> market_data_reader(
        stub_->MarketData(&context_, MDRequest())
    );
    while (market_data_reader->Read(&market_data_)) {
        serialiseMarketData();
        for (const auto& subscriber : subscribers_) {
            socket_.async_send_to(
                boost::asio::buffer(temp_buffer_, temp_buffer_[0] + 2),
                subscriber,
                [this, subscriber](boost::system::error_code ec, std::size_t) {
                    if (ec) {
                        this->subscribers_.erase(
                            std::find(
                                subscribers_.begin(), 
                                subscribers_.end(),
                                subscriber
                            )
                        );
                    }
                }
            );
        }
    }
    acceptloop.join();
}

void DataPlatform::serialiseMarketData() {
    switch(market_data_.OrderEntryType_case()) {
        case type::kAdd:
            serialiseAddOrder();
            break;
        case type::kCancel:
            serialiseCancel();
            break;
        case type::kFill:
            serialiseFill();
            break;
        case type::kMod:
            serialiseModOrder();
            break;
        case type::kNotification:
            serialiseNotification();
            break;
        case type::ORDERENTRYTYPE_NOT_SET:
            break;
    }
}

void DataPlatform::serialiseAddOrder() {
    char* temp_ptr = temp_buffer_.data();
    *(temp_ptr++) = add_data_len_;
    *(temp_ptr++) = 'A';
    serialiseBytes(temp_ptr, market_data_.add().timestamp());
    serialiseBytes(temp_ptr, market_data_.add().order_id());
    serialiseBytes(temp_ptr, market_data_.add().ticker());
    serialiseBytes(temp_ptr, market_data_.add().price());
    serialiseBytes(temp_ptr, market_data_.add().quantity());
    serialiseBytes(temp_ptr, market_data_.add().is_buy_side());
}

void DataPlatform::serialiseModOrder() {
    char* temp_ptr = temp_buffer_.data();
    *(temp_ptr++) = mod_data_len_;
    *(temp_ptr++) = 'M';
    serialiseBytes(temp_ptr, market_data_.mod().timestamp());
    serialiseBytes(temp_ptr, market_data_.mod().order_id());
    serialiseBytes(temp_ptr, market_data_.mod().quantity());
}

void DataPlatform::serialiseFill() {
    char* temp_ptr = temp_buffer_.data();
    *(temp_ptr++) = fill_data_len_;
    *(temp_ptr++) = 'F';
    serialiseBytes(temp_ptr, market_data_.fill().timestamp());
    serialiseBytes(temp_ptr, market_data_.fill().status_common().order_id());
    serialiseBytes(temp_ptr, market_data_.fill().status_common().ticker());
    serialiseBytes(temp_ptr, market_data_.fill().fill_id());
    serialiseBytes(temp_ptr, market_data_.fill().fill_quantity());
}

void DataPlatform::serialiseNotification() {
    char* temp_ptr = temp_buffer_.data();
    *(temp_ptr++) = notification_len_;
    *(temp_ptr++) = 'N';
    serialiseBytes(temp_ptr, market_data_.notification().timestamp());
    serialiseBytes(temp_ptr, market_data_.notification().flag());
}

void DataPlatform::serialiseCancel() {
    char* temp_ptr = temp_buffer_.data();
    *(temp_ptr++) = cancel_data_len_;
    *(temp_ptr++) = 'C';
    serialiseBytes(temp_ptr, market_data_.cancel().timestamp());
    serialiseBytes(temp_ptr, market_data_.cancel().order_id());
}

void DataPlatform::acceptSubscriber() {
    socket_.async_receive_from(
        boost::asio::buffer(conn_buffer_, 1),
        temp_remote_endpoint_,
        [this](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                this->subscribers_.insert(this->temp_remote_endpoint_);
                this->acceptSubscriber();
            }
        }
    );
}
