#ifndef SERVER_CONNECTION_HPP
#define SERVER_CONNECTION_HPP

#include <string>
#include <boost/asio.hpp>

#include "order.hpp"
#include "ordertypes.hpp"
#include "tempbuffer.hpp"
#include "ordermanager.hpp"

namespace server {
class ServerConnection;
using tcp = boost::asio::ip::tcp;
using ThisShared = std::enable_shared_from_this<ServerConnection>;
class ServerConnection : public ThisShared {
public:
    ServerConnection(tcp::socket socket, tradeorder::OrderManager& ordermanager);
    void init();
private:
    void readOrderHeader();
    void readOrderBody();
    void interpretOrderType();
    void addOrder();
    void modifyOrder();
    void cancelOrder();
    void replaceOrder();
    tcp::socket socket_;
    std::string ip_;
    util::TempBuffer tbuffer_;
    tradeorder::OrderManager& ordermanager_;
};
}

#endif
