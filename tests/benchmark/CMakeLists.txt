cmake_minimum_required(VERSION 3.16)

find_package(benchmark REQUIRED)

add_executable(orderbook_benchmark
    orderbookbenchmark.cpp
    ${PROJECT_SOURCE_DIR}/src/server/order/orderbookmanager.cpp
    ${PROJECT_SOURCE_DIR}/src/server/order/orderbook.cpp
)

target_link_libraries(orderbook_benchmark PRIVATE benchmark::benchmark)
target_include_directories(orderbook_benchmark PUBLIC ${tradeserver_inc})
target_compile_definitions(orderbook_benchmark PUBLIC -DTEST_BUILD)
target_compile_options(orderbook_benchmark PUBLIC "-std=c++17" -O3 -g)

add_executable(serverbencher serverbencher.cpp)
target_link_libraries(serverbencher
    ${Boost_LIBRARIES} 
    oe_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
