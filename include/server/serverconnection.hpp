#ifndef SERVER_CONNECTION_HPP
#define SERVER_CONNECTION_HPP

#include <string>
#include <boost/asio.hpp>

#include "message.hpp"

namespace server {
    using tcp = boost::asio::ip::tcp;
    using ThisShared = std::enable_shared_from_this<ServerConnection>;
class ServerConnection : public ThisShared {
public:
    ServerConnection(tcp::socket socket, std::string ip);
    void init();
private:
    void readMsgHeader();
    void readMsgBody();
    tcp::socket socket_;
    std::string ip_;
    msg::Message temp_msg_;
};
}

#endif
