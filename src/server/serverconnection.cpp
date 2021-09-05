#include "serverconnection.hpp"

using namespace server;

ServerConnection::ServerConnection(tcp::socket socket, std::string ip) :
    socket_(std::move(socket)), ip_(ip) 
{}

void ServerConnection::init() {
    readOrderHeader();
}

void ServerConnection::readOrderHeader() {
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(
            temp_buffer_,
            tradeorder::HEADER_LEN
        ),
        [self(shared_from_this())](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                self->readOrderBody();
            }
            else
                self->socket_.close();
        }
    );
}

void ServerConnection::readOrderBody() {
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(
            temp_buffer_ + tradeorder::HEADER_LEN,
            tradeorder::MAX_BODY_LEN - tradeorder::HEADER_LEN
        ),
        [self(shared_from_this())](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                self->interpretOrderType();
                self->readOrderHeader();
            }
            else
                self->socket_.close();
        }
    );
}

void ServerConnection::interpretOrderType() {
    switch(temp_buffer_[0]) {
        case 'A':
            break;
        case 'M':
            break;
        case 'C':
            break;
        case 'R':
            break;
        default:
            break;
    }
}
