#include "client.hpp"

using namespace client;

TradingClient::TradingClient(std::shared_ptr<grpc::Channel> channel, const char* dp_hostname, const char* dp_port)
  : stub_(orderentry::OrderEntryService::NewStub(channel)) 
  , socket_(io_context_, udp::endpoint(udp::v4(), std::atoi(dp_port)))
  , resolver_(io_context_)
{
    subscribeToDataPlatform(dp_hostname, dp_port);
}

void TradingClient::startOrderEntry() {
    grpc::ClientContext context;
    std::shared_ptr<
        grpc::ClientReaderWriter<OERequest, OEResponse>
    >oe_stream(stub_->OrderEntry(&context));
    std::thread oe_writer([this, oe_stream]() {
        OERequest request;
        makeNewOrderRequest(request);
        oe_stream->Write(request);
        for(;;) {
            
        }
    });
    OEResponse oe_response;
    while(oe_stream->Read(&oe_response)) {
        interpretResponseType(oe_response);
    }
    oe_writer.join();
    grpc::Status status = oe_stream->Finish();
    if (!status.ok()) {
        std::cout << "Order Entry RPC failed" << std::endl;
    }
}

void TradingClient::subscribeToDataPlatform(const char* dp_hostname, const char* dp_port) {
    udp::resolver::results_type endpoints = resolver_.resolve(
        udp::v4(), dp_hostname, dp_port
    );
    marketdata_platform_ = *endpoints.begin();
    std::thread([&](){io_context_.run();});
    socket_.async_connect(marketdata_platform_, 
        [self(shared_from_this())](boost::system::error_code ec) {
            if (!ec)
                self->readMarketData();
        }
    );
}

void TradingClient::readMarketData() {
    socket_.async_receive(
        boost::asio::buffer(buffer_, 255),
        [self(shared_from_this())](boost::system::error_code ec, std::size_t){
            if (!ec) {
                // update orderbooks etc
            }
            self->readMarketData();
        }
    );
}

void TradingClient::interpretResponseType(OEResponse& oe_response) {
    switch(oe_response.OrderStatusType_case()) {
        case AckType::kNewOrderAck:
            std::cout << "Got new order ack\n";
            break;
        case AckType::kModifyOrderAck:
            break;
        case AckType::kCancelOrderAck:
            break;
        case AckType::ORDERSTATUSTYPE_NOT_SET:
            switch(oe_response.ResponseType_case()) {
            case RespType::kFill:
                break;
            case RespType::kRejection:
                std::cout << "Got rejection\n";
                break;
            }
            break;
        default:
            std::cout << "default case\n";
            break;
    }
}

bool TradingClient::getUserInput() {
    std::string input;
    std::cin >> input;
    auto orderpos = input.find("-order ");
    if (orderpos == std::string::npos) {
        return false;
    }
}

void TradingClient::makeNewOrderRequest(OERequest& request) {
    auto neworder = request.mutable_new_order();
    auto common = neworder->mutable_order_common();
    neworder->set_is_buy_side(1);
    neworder->set_price(200);
    neworder->set_quantity(100);
    common->set_ticker(123);
    common->set_user_id(20);
    common->set_order_id(20);
}
