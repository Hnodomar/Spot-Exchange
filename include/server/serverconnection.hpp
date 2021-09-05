#ifndef SERVER_CONNECTION_HPP
#define SERVER_CONNECTION_HPP

#include <string>
#include <boost/asio.hpp>

#include "order.hpp"

namespace server {
    class ServerConnection;
    using tcp = boost::asio::ip::tcp;
    using ThisShared = std::enable_shared_from_this<ServerConnection>;
class ServerConnection : public ThisShared {
public:
    ServerConnection(tcp::socket socket, std::string ip);
    void init();
private:
    void readOrderHeader();
    void readOrderBody();
    void interpretOrderType();
    tcp::socket socket_;
    std::string ip_;
    uint8_t temp_buffer_[tradeorder::HEADER_LEN + tradeorder::MAX_BODY_LEN] = {0};
};
}

#endif
