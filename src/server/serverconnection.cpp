#include "serverconnection.hpp"

using namespace server;

ServerConnection::ServerConnection(tcp::socket socket, tradeorder::OrderManager& om)
    : socket_(std::move(socket)), ip_(socket.remote_endpoint().address().to_string()),
      ordermanager_(om) 
{}

void ServerConnection::init() {
    readOrderHeader();
}

void ServerConnection::readOrderHeader() {
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(
            tbuffer_.getBuffer(),
            ::tradeorder::HEADER_LEN
        ),
        [self(shared_from_this())](boost::system::error_code ec, std::size_t) {
            if (!ec && self->tbuffer_.parseHeader()) {
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
            tbuffer_.getBodyBuffer(),
            tbuffer_.getBodyLength()
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
    switch(tbuffer_.getOrderType()) {
        case 'A':
            addOrder();
            break;
        case 'M':
            modifyOrder();
            break;
        case 'C':
            cancelOrder();
            break;
        case 'R':
            replaceOrder();
            break;
        default:
            break;
    }
}

void ServerConnection::addOrder() {
    ordermanager_.addOrder(::tradeorder::Order(
        reinterpret_cast<::tradeorder::AddOrder*>(tbuffer_.getBodyBuffer())
    ));
}
