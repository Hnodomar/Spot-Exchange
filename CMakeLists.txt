cmake_minimum_required(VERSION 3.16)
project(TradingEngine)

set(CMAKE_CXX_FLAGS "-std=c++17 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(BOOST_MIN_VERSION "1.70.0")

find_package(Boost REQUIRED COMPONENTS iostreams)

file(GLOB server_src src/server/*.cpp)

add_executable(tradeserver ${server_src})
target_link_libraries(tradeserver ${Boost_LIBRARIES})

include_directories(${Boost_INCLUDE_DIR})
