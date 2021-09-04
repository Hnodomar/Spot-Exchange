#ifndef SERVER_CONNECTION_HPP
#define SERVER_CONNECTION_HPP

#include <string>
#include <boost/asio.hpp>

#include "message.hpp"

using tcp = boost::asio::ip::tcp;

namespace server {
class ServerConnection {
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
