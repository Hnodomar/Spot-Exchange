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
        promptUserID();
        printInfoBox();
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
    AddOrderData* add_order = reinterpret_cast<AddOrderData*>(buffer_ + HEADER_LEN);
    feedhandler_.addOrder(add_order);
    if (subscription_ != nullptr) {
        if (subscription_->getTicker() == add_order->ticker) {
            reprintInterface();
        }
    }
}

void TradingClient::processModifyOrderData() {
    ModOrderData* mod_order = reinterpret_cast<ModOrderData*>(buffer_ + HEADER_LEN);
    feedhandler_.modifyOrder(mod_order);
    if (subscription_ != nullptr) {
        auto tkr = feedhandler_.getOrderIDTicker(mod_order->order_id);
        if (subscription_->getTicker() == tkr) {
            reprintInterface();
        }
    }
}

void TradingClient::processCancelOrderData() {
    CancelOrderData* cancel_order = reinterpret_cast<CancelOrderData*>(buffer_ + HEADER_LEN);
    feedhandler_.cancelOrder(cancel_order);
    if (subscription_ != nullptr) {
        auto tkr = feedhandler_.getOrderIDTicker(cancel_order->order_id);
        if (subscription_->getTicker() == tkr) {
            reprintInterface();
        }
    }
}

void TradingClient::processFillOrderData() {
    FillOrderData* fill = reinterpret_cast<FillOrderData*>(buffer_ + HEADER_LEN);
    feedhandler_.fillOrder(fill);
    if (subscription_ != nullptr) {
        if (subscription_->getTicker() == fill->ticker) {
            reprintInterface();
        }
    }
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

void TradingClient::reprintInterface() {
    std::cout << "\033[2J\033[1;1H";
    if (subscription_ != nullptr)
        std::cout << *subscription_ << std::endl;
    printInfoBox(false);
    std::cout << ">";
}

void TradingClient::printInfoBox(bool clear_prev) {
    if (clear_prev) {
        for (int i = 0; i < prev_height_; ++i) {
            std::cout << "\033[A" << "\33[2K";
        }
    }
    auto width = util::getTerminalWidth();
    auto height = util::getTerminalHeight();
    height /= 5;
    prev_height_ = height + 4;
    std::cout << std::setfill('-') << std::setw(width - 2) << centered("INFO") << std::endl;
    std::cout << std::setfill(' ') << "\n";
    for (auto itr = info_feed_.rbegin(); itr != info_feed_.rbegin() + height; ++itr) {
        std::cout << std::setw(width) << centered(*itr) << std::endl;
    }
    std::cout << std::setfill('-') << std::setw(width);
    std::cout << "\n";
    while (info_feed_.size() > 20)
        info_feed_.erase(info_feed_.begin());
}

void TradingClient::promptUserID() {
    if (userID_ == 0)
        std::cout << "Please input user ID: " << std::endl;
    else
        std::cout << "User ID " << util::convertEightBytesToString(userID_) 
            << " id taken. Please input new user ID: " << std::endl;
    std::cout << ">";
    std::string username;
    getline(std::cin, username);
    userID_ = util::convertStrToEightBytes(username);
    removeUserInput();
    std::cout << std::endl;
}

void TradingClient::interpretAck(const NewOrderAck& new_ack) {
    const auto& ord = new_ack.new_order();
    std::string side = ord.is_buy_side() == 1 ? "BID" : "ASK";
    std::string tkr = util::convertEightBytesToString(ord.order_common().ticker());
    info_feed_.push_back(
        timestampStr(new_ack.timestamp())
        + "ADD " + side + " TO " + tkr + " "
        + std::to_string(ord.quantity()) + " SHARES @ "
        + "£" + std::to_string(ord.price())
        + " ACKNOWLEDGED "
        + "WITH ID " + std::to_string(ord.order_common().order_id())
    );
    printInfoBox();
}

void TradingClient::interpretAck(const ModOrderAck& mod_ack) {
    const auto& ord = mod_ack.modify_order();
    std::string tkr = util::convertEightBytesToString(ord.order_common().ticker());
    auto curr_ord = feedhandler_.getOrder(ord.order_common().order_id());
    std::string side = ord.is_buy_side() == 1 ? "BID" : "ASK";
    info_feed_.push_back(
        timestampStr(mod_ack.timestamp())
        + "MODIFY " + side + " IN " + tkr + " "
        + "FROM" + std::to_string(curr_ord->quantity) + " SHARES "
        + "TO" + std::to_string(ord.quantity()) + " SHARES @ £"
        + std::to_string(ord.price()) + " ACKNOWLEDGED WITH ID "
        + std::to_string(ord.order_common().order_id())
    );
    printInfoBox();
}

void TradingClient::interpretAck(const CancelOrderAck& cancel_ack) {
    const auto& ord = cancel_ack.status_common();
    auto curr_ord = feedhandler_.getOrder(ord.order_id());
    std::string tkr = util::convertEightBytesToString(ord.ticker());
    std::string side = curr_ord->is_buy_side == 1 ? "BID" : "ASK";
    info_feed_.push_back(
        timestampStr(cancel_ack.timestamp())
        + "CANCEL " + side + " IN " + tkr + " "
        + "ACKNOWLEDGED WITH ID "
        + std::to_string(ord.order_id())
    );
    printInfoBox();
}

void TradingClient::interpretAck(const FillAck& fill_ack) {
    const auto& ord = fill_ack.status_common();
    if (ord.user_id() == getUserID()) {
        auto curr_ord = feedhandler_.getOrder(ord.order_id());
        std::string side;
        if (curr_ord != nullptr)
            side = curr_ord->is_buy_side == 1 ? "BID" : "ASK";
        std::string tkr = util::convertEightBytesToString(ord.ticker());
        info_feed_.push_back(
            timestampStr(fill_ack.timestamp())
            + side + " ORDER " + std::to_string(ord.order_id())
            + " IN " + tkr + " FILLED: " + std::to_string(fill_ack.fill_quantity())
            + " / " + std::to_string(curr_ord->quantity)
        );
        printInfoBox();
    }
}

void TradingClient::interpretAck(const RejectAck& reject_ack) {
    info_feed_.push_back(
        "ORDER ENTRY REJECTED! REASON: " 
        + rejectionToString(reject_ack.rejection_response())
    );
    printInfoBox();
}

std::string TradingClient::commonStr(const Common& common) {
    return "ORDER: " + std::to_string(common.order_id()) + " TICKER: " 
        + util::convertEightBytesToString(common.ticker());
}

std::string TradingClient::commonStr(int64_t timestamp, const Common& common) {
    return timestampStr(timestamp) + commonStr(common);
}

std::string TradingClient::timestampStr(int64_t timestamp) const {
    return "[" + util::getTimeStringFromTimestamp(timestamp) + "] ";
}

std::string TradingClient::rejectionToString(const RejectType rejection) const {
    switch (rejection) {
        case RejectType::OrderEntryRejection_RejectionReason_unknown:
            return "UNKNOWN";
        case RejectType::OrderEntryRejection_RejectionReason_order_not_found:
            return "ORDER NOT FOUND";
        case RejectType::OrderEntryRejection_RejectionReason_order_id_already_present:
            return "ORDER ID ALREADY PRESENT";
        case RejectType::OrderEntryRejection_RejectionReason_orderbook_not_found:
            return "ORDERBOOK NOT FOUND";
        case RejectType::OrderEntryRejection_RejectionReason_ticker_not_found:
            return "TICKER NOT FOUND";
        case RejectType::OrderEntryRejection_RejectionReason_modify_wrong_side:
            return "MODIFY WRONG SIDE";
        case RejectType::OrderEntryRejection_RejectionReason_modification_trivial:
            return "MODIFICATION TRIVIAL";
        case RejectType::OrderEntryRejection_RejectionReason_wrong_user_id:
            return "WRONG USER ID";
        default:
            return "";
    }
}

bool TradingClient::getUserInput(OERequest& request) {
    std::string input;
    std::cout << "> ";
    std::getline(std::cin, input);
    std::cout << std::endl;
    removeUserInput();
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
    auto args = split(command, ' ');
    if (args[0] == "/subscribe") {
        subscription_ = feedhandler_.subscribe(util::convertStrToEightBytes(args[1]));
        if (subscription_ == nullptr)
            std::cout << "Subscription failed!" << std::endl;
        else
            reprintInterface();
    }
    else if (args[0] == "/help") {

    }
    else
        std::cout << "Invalid command" << std::endl;
    return true;
}

std::vector<std::string> TradingClient::split(const std::string& s, char delimiter) {
    std::vector<std::string> out{};
	std::stringstream ss {s};
	std::string item;	
	while (std::getline(ss, item, delimiter))
			out.push_back(item);
	return out;
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
    common->set_user_id(userID_);
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
    common->set_user_id(userID_);
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
    common->set_user_id(userID_);
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
