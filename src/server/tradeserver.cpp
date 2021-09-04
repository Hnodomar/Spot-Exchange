#include "tradeserver.hpp"

using namespace server;

TradeServer::TradeServer(char* port, const std::string& outputfile="") :
listener_(ioctxt_, tcp::endpoint(tcp::v4(), std::atoi(port))), 
logger_(outputfile) {
    logger_.write("[ SERVER ]: Successfully setup server on "
     + listener_.local_endpoint().address().to_string() 
     + ":" + std::to_string(listener_.local_endpoint().port())
    );
    acceptConnections();    
}

void TradeServer::acceptConnections() {
    listener_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<ServerConnection>(
                    std::move(socket),
                    socket.remote_endpoint().address().to_string()
                )->init();
            }
            acceptConnections();
        }
    );
}
