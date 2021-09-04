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
        [self(shared_from_this())](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                self->readMsgBody();
            }
            else
                self->socket_.close();
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
        [self(shared_from_this())](boost::system::error_code ec, std::size_t) {
            if (!ec)
                self->readMsgHeader();
            else
                self->socket_.close();
        }
    );
}
