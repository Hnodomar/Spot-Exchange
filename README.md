# Spot Exchange

A multi-threaded exchange with engine and FIFO limit orderbook built in C++, leveraging gRPC to present users an interface to submit several kinds of orders. Clients can listen to a separate market data platform without the need to connect to the order entry server, and receive live market data via UDP.

## Features

### Order Types

* Add Order
* Modify Order
* Cancel Order

### Order Responses

* Add Order Acknowledgement
* Modify Order Acknowledgement
* Cancel Order Acknowledgement

### Market Data

Loosely inspired by NASDAQ ITCH-50 (Currently no sequence numbers)

* Order Added
* Order Modified
* Order Cancelled
* Order Filled
* Market Notification

### Low Latency Logging

The trading platform keeps a comprehensive log record of all clients and actions that the server takes, including order entry data. To achieve this in a multi-threaded implementation, a custom Multi-Producer Single-Consumer (MPSC) lockless queue was designed and implemented to maintain low latency with a large amount of logging. False-sharing is avoided by requesting strict alignment requirements on relevant objects.

## Trading Engine Architecture

The diagram below represents the overall design of the Trading Engine. 

![](description/TradingEngine.png)

* Establishing a bi-directional gRPC stream of order requests and responses between the server and client allows for fast and secure communication.

* A separate market data platform means that clients do not have to establish a connection with the order entry server in order to receive live market data. Market data is provided over UDP, leveraging Boost ASIO and a custom serialisation.

* Planned: a Cassandra DB instance is used to persist relevant data for future usage, and potentially reconstruction.

### Operation Flow

1. Clients interested in submitting order entries connect to Order Entry Server via TCP setting up a bi-directional gRPC stream.

1. Clients can submit Add Order, Cancel Order and Modify Order requests and receive corresponding order acknowledgements for each.

1. If a client's order request is filled they receive a fill response over the bi-directional stream, and if not fully filled is added to the orderbook.

1. Fills and order book modifications generate market data that is sent to the market data platform via unary gRPC stream.

1. The generated market data is disseminated among all listening clients via UDP. Listening clients can include order entry clients and non-order entry clients such as data feeds.

## Dependencies

* gRPC https://grpc.io/
* Google Protocol Buffers https://developers.google.com/protocol-buffers
* Boost (version > 1.70.0) https://www.boost.org/
* CMake https://cmake.org/
