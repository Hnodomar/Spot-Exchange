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
                continue;
            }
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
    std::array<char, 1> conn_req = {0};
    socket_.send_to(boost::asio::buffer(conn_req), marketdata_platform_);   
    readMarketData();
    threads_.emplace_back(([&](){io_context_.run();}));
}

void TradingClient::readMarketData() {
    socket_.async_receive_from(
        boost::asio::buffer(buffer_),
        marketdata_platform_,
        [this](boost::system::error_code ec, std::size_t){
            if (!ec) {
                data_length_ = buffer_[0];
                packet_type_ = buffer_[1];
                processMarketData();
            }
            readMarketData();
        }
    );
}

void TradingClient::processMarketData() {
    switch(packet_type_) {
        case 'A':
            processAddOrderData();
            break;
        case 'M':
            processModifyOrderData();
            break;
        case 'C':
            processCancelOrderData();
            break;
        case 'F':
            processFillOrderData();
            break;
        case 'N':
            processNotificationData();
            break;
        default:
            std::cout << "incorrect type" << std::endl;
            break;
    }
}

void TradingClient::processAddOrderData() {
    feedhandler_.addOrder(reinterpret_cast<AddOrderData*>(buffer_));
}

void TradingClient::processModifyOrderData() {
    feedhandler_.modifyOrder(reinterpret_cast<ModOrderData*>(buffer_));
}

void TradingClient::processCancelOrderData() {
    feedhandler_.cancelOrder(reinterpret_cast<CancelOrderData*>(buffer_));
}

void TradingClient::processFillOrderData() {
    feedhandler_.fillOrder(reinterpret_cast<FillOrderData*>(buffer_));
}

void TradingClient::processNotificationData() {

}

void TradingClient::interpretResponseType(OEResponse& oe_response) {
    switch(oe_response.OrderStatusType_case()) {
        case AckType::kNewOrderAck:
            interpretAck(oe_response.new_order_ack());
            break;
        case AckType::kModifyOrderAck:
            interpretAck(oe_response.modify_order_ack());
            break;
        case AckType::kCancelOrderAck:
            interpretAck(oe_response.cancel_order_ack());
            break;
        case AckType::ORDERSTATUSTYPE_NOT_SET:
            switch(oe_response.ResponseType_case()) {
                case RespType::kFill:
                    interpretAck(oe_response.fill());
                    break;
                case RespType::kRejection:
                    interpretAck(oe_response.rejection());
                    break;
                case RespType::RESPONSETYPE_NOT_SET:
                    break;
            }
            break;
    }
}

void TradingClient::interpretAck(const NewOrderAck& new_ack) const {
    const auto& ord = new_ack.new_order();
    std::cout << "Received new order acknowledgement:\n";
    printCommon(new_ack.timestamp(), ord.order_common());
    std::cout << " " << ord.price() << " " << ord.quantity() << " " << 
        ord.is_buy_side() << std::endl;
}

void TradingClient::interpretAck(const ModOrderAck& mod_ack) const {
    const auto& ord = mod_ack.modify_order();
    std::cout << "Received modify order acknowledgement:\n";
    printCommon(mod_ack.timestamp(), ord.order_common());
    std::cout << " " << ord.price() << " " 
        << ord.quantity() << " " << ord.is_buy_side() << std::endl;
}

void TradingClient::interpretAck(const CancelOrderAck& cancel_ack) const {
    std::cout << "Received cancel order acknowledgement:\n";
    printCommon(cancel_ack.timestamp(), cancel_ack.status_common());
    std::cout << std::endl;
}

void TradingClient::interpretAck(const FillAck& fill_ack) const {
    std::cout << "Order received fill:\n";
    printCommon(fill_ack.timestamp(), fill_ack.status_common());
    std::cout << " " << fill_ack.fill_quantity() << " " << fill_ack.fill_id()
        << std::endl;
}

void TradingClient::interpretAck(const RejectAck& reject_ack) const {
    std::cout << "Order entry rejected:\n"
        << "Reason: " << rejectionToString(reject_ack.rejection_response()) << " ";
    printCommon(reject_ack.order_common());
    std::cout << std::endl;
}

void TradingClient::printCommon(const Common& common) const {
    std::cout << "Order ID: " << common.order_id()
     << " User ID: " << util::convertEightBytesToString(common.user_id())
     << " Ticker: " << util::convertEightBytesToString(common.ticker());
}

void TradingClient::printCommon(int64_t timestamp, const Common& common) const {
    printTimestamp(timestamp);
    printCommon(common);
}

void TradingClient::printTimestamp(int64_t timestamp) const {
    std::cout << "[" << util::getTimeStringFromTimestamp(timestamp) << "] ";
}

std::string TradingClient::rejectionToString(const RejectType rejection) const {
    switch (rejection) {
        case RejectType::OrderEntryRejection_RejectionReason_unknown:
            return "unknown";
        case RejectType::OrderEntryRejection_RejectionReason_order_not_found:
            return "order not found";
        case RejectType::OrderEntryRejection_RejectionReason_order_id_already_present:
            return "order id already present";
        case RejectType::OrderEntryRejection_RejectionReason_orderbook_not_found:
            return "orderbook not found";
        case RejectType::OrderEntryRejection_RejectionReason_ticker_not_found:
            return "ticker not found";
        case RejectType::OrderEntryRejection_RejectionReason_modify_wrong_side:
            return "modify wrong side";
        case RejectType::OrderEntryRejection_RejectionReason_modification_trivial:
            return "modification trivial";
        case RejectType::OrderEntryRejection_RejectionReason_wrong_user_id:
            return "wrong user id";
        default:
            return "";
    }
}

bool TradingClient::getUserInput(OERequest& request) {
    std::cout << "Provide Input:" << std::endl;
    std::string input;
    std::getline(std::cin, input);
    input += " ";
    if (userEnteredCommand(input)) {
        return false;
    }
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

bool TradingClient::userEnteredCommand(const std::string& command) {
    if (command[0] != '/')
        return false;
    auto cmd_end_pos = command.find(" ");
    if (cmd_end_pos == std::string::npos)
        return false;
    auto cmd = command.substr(1, cmd_end_pos);
    if (cmd == "subscribe") {
        auto tkr_end_pos = command.find(" ", cmd_end_pos + 1);
        if (tkr_end_pos == std::string::npos)
            return true;
        auto tkr = command.substr(cmd_end_pos + 1, tkr_end_pos);
        subscription_ = feedhandler_.subscribe(util::convertStrToEightBytes(tkr));
    }
    else if (cmd == "help") {

    }
    else
        std::cout << "Invalid command" << std::endl;
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
