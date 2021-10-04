# Trading Engine

A multi-threaded algorithmic trading platform with engine and FIFO limit orderbook built in C++, leveraging gRPC to present users an interface to submit several kinds of orders. Clients can listen to a separate market data platform without the need to connect to the order entry server, and receive live market data via UDP.

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

Loosely inspired by NASDAQ ITCH-50

* Order Added
* Order Modified
* Order Cancelled
* Order Filled
* Market Notification

### Low Latency Logging

The trading platform keeps a comprehensive log record of all clients and actions that the server takes, including order entry data. To achieve this in a multi-threaded implementation, a custom Multi-Producer Single-Consumer (MPSC) lockless queue was designed and implemented to maintain low latency with a large amount of logging. False-sharing is avoided by requesting strict alignment requirements on relevant objects.


![](description/TradingEngine.png)


## Dependencies

* gRPC https://grpc.io/
* Google Protocol Buffers https://developers.google.com/protocol-buffers
* Boost (version > 1.70.0) https://www.boost.org/
* CMake https://cmake.org/
