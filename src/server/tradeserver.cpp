#include "tradeserver.hpp"

using namespace server;

TradeServer::TradeServer(char* port) :
listener_(ioctxt_, tcp::endpoint(tcp::v4(), std::atoi(port))) {
    acceptConnections();    
}

void TradeServer::acceptConnections() {
    listener_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<ServerConnection>(
                    socket,
                    socket.remote_endpoint().address().to_string()
                )->init();
            }
            acceptConnections();
        }
    );
}
