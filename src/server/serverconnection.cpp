#include "serverconnection.hpp"

using namespace server;

ServerConnection::ServerConnection(tcp::socket socket, std::string ip) :
socket_(std::move(socket)), ip_(ip) {
    readMsgHeader();
}

void ServerConnection::init() {
    readMsgHeader();
}

void ServerConnection::readMsgHeader() {
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(
            temp_msg_.getMsgHeader(),
            msg::HEADER_LEN
        ),
        [this](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                readMsgBody();
            }
            else
                socket_.close();
        }
    );
}

void ServerConnection::readMsgBody() {
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(
            temp_msg_.getMsgBody(),
            temp_msg_.getBodyLen()
        ),
        [this](boost::system::error_code ec, std::size_t) {
            if (!ec)
                readMsgHeader();
            else
                socket_.close();
        }
    );
}
