#ifndef TRADE_SERVER_HPP
#define TRADE_SERVER_HPP

#include <boost/asio.hpp>

namespace server {
using tcp = boost::asio::ip::tcp;
class TradeServer {
public:
    TradeServer(char* port);
    ~TradeServer();
private:
    void acceptConnections();
    boost::asio::io_context ioctxt_;
    tcp::acceptor listener_;
};
}

#endif
