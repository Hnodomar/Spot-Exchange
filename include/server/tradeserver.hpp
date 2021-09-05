#ifndef TRADE_SERVER_HPP
#define TRADE_SERVER_HPP

#include <boost/asio.hpp>
#include <memory>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "logger.hpp"
#include "serverconnection.hpp"
#include "ordermanager.hpp"

namespace server {
using tcp = boost::asio::ip::tcp;
class TradeServer {
public:
    TradeServer(char* port, const std::string& filename);
    ~TradeServer();
private:
    void acceptConnections();
    boost::asio::io_context ioctxt_;
    tcp::acceptor listener_;
    logging::Logger logger_;
    tradeorder::OrderManager ordermanager_;
};
}

#endif
