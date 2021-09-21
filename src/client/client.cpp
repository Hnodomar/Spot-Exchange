#include "client.hpp"

using namespace client;

TradingClient::TradingClient(std::shared_ptr<grpc::Channel> channel, const char* dp_hostname, const char* dp_port)
  : stub_(orderentry::OrderEntryService::NewStub(channel)) 
  , local_endpoint_(udp::endpoint(udp::v4(), 9003))
  , socket_(io_context_, local_endpoint_)
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
        for(;;) {
            OERequest request;
            if (!getUserInput(request)) {
                std::cout << "bad input" << std::endl;
                continue;
            }
            std::cout << "sending request" << std::endl;
            oe_stream->Write(request);
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
    socket_.async_connect(marketdata_platform_, 
        [this](boost::system::error_code ec) {
            if (!ec) {
                std::cout << "successfully connected!\n";
                readMarketData();
            }
        }
    );
    threads_.emplace_back(([&](){io_context_.run();}));
}

void TradingClient::readMarketData() {
    socket_.async_receive(
        boost::asio::buffer(buffer_, 255),
        [this](boost::system::error_code ec, std::size_t){
            if (!ec) {
                // update orderbooks etc
            }
            readMarketData();
        }
    );
}

void TradingClient::interpretResponseType(OEResponse& oe_response) {
    switch(oe_response.OrderStatusType_case()) {
        case AckType::kNewOrderAck:
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
                    break;
                case RespType::RESPONSETYPE_NOT_SET:
                    break;
            }
            break;
    }
}



bool TradingClient::getUserInput(OERequest& request) {
    std::cout << "Provide Input:" << std::endl;
    std::string input;
    std::getline(std::cin, input);
    input += " ";
    auto orderpos = input.find("-order ");
    if (orderpos == std::string::npos) {
        return false;
    }
    auto ordertypepos = input.find(" ", orderpos);
    auto end_ordertypepos = input.find(" ", ordertypepos + 1);
    if (ordertypepos == std::string::npos || end_ordertypepos == std::string::npos) {
        return false;
    }
    auto ordertype = input.substr(ordertypepos + 1, end_ordertypepos - (ordertypepos + 1));
    if (ordertype == "add") {
        if (!constructNewOrderRequest(request, input)) {
            return false;
        }
        return true;
    }
    else if (ordertype == "cancel") {
        if (!constructModifyOrderRequest(request, input))
            return false;
    }
    else if (ordertype == "modify") {
        if (!constructCancelOrderRequest(request, input))
            return false;
    }
    else {
        std::cout << "Invalid order type" << std::endl;
        return false;
    }
    return true;
}

bool TradingClient::constructNewOrderRequest(OERequest& request, const std::string& input) {
    auto neworder = request.mutable_new_order();
    auto common = neworder->mutable_order_common();
    std::vector<std::string> values(getOrderValues(input, addorder_fields_));
    if (values.empty())
        return false;
    if (!setSide(neworder, values[0][0]))
        return false;
    neworder->set_price(std::stoi(values[1]));
    neworder->set_quantity(std::stoi(values[2]));
    common->set_ticker(util::convertStrToEightBytes(values[3]));
    common->set_user_id(util::convertStrToEightBytes(values[4]));
    common->set_order_id(0);
    return true;
}

bool TradingClient::constructModifyOrderRequest(OERequest& request, const std::string& input) {
    auto modorder = request.mutable_new_order();
    auto common = modorder->mutable_order_common();
    std::vector<std::string> values(getOrderValues(input, modorder_fields_));
    if (values.empty())
        return false;
    if (!setSide(modorder, values[0][0]))
        return false;
    modorder->set_price(std::stoi(values[1]));
    modorder->set_quantity(std::stoi(values[2]));
    common->set_ticker(util::convertStrToEightBytes(values[3]));
    common->set_user_id(util::convertStrToEightBytes(values[4]));
    common->set_order_id(std::stoi(values[5]));
    return true;
}

bool TradingClient::constructCancelOrderRequest(OERequest& request, const std::string& input) {
    auto cancelorder = request.mutable_new_order();
    auto common = cancelorder->mutable_order_common();
    std::vector<std::string> values(getOrderValues(input, cancelorder_fields_));
    if (values.empty())
        return false;
    common->set_ticker(util::convertStrToEightBytes(values[0]));
    common->set_user_id(util::convertStrToEightBytes(values[1]));
    common->set_order_id(std::stoi(values[2]));
    return true;
}

inline std::vector<std::string> TradingClient::getOrderValues(const std::string& input, 
const std::vector<std::string>& fields) {
    std::vector<std::string> values;
    for (const auto& field : fields) {
        auto value = findField(field, input);
        if (value == "") {
            return std::vector<std::string>();
        }
        values.push_back(value);
    }
    return values;
}

std::string TradingClient::findField(const std::string& field, const std::string& input) const {
    auto fieldpos = input.find(field);
    if (fieldpos == std::string::npos) {
        return "";
    }
    auto valpos = input.find(' ', fieldpos);
    auto end_valpos = input.find(" ", valpos + 1);
    if (valpos == std::string::npos) {
        return "";
    }
    if (end_valpos == std::string::npos)
        return input.substr(valpos + 1, input.length() - (valpos + 1));
    auto val = input.substr(valpos + 1, end_valpos - (valpos + 1));
    return val;
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
