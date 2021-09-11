cmake_minimum_required(VERSION 3.16)
project(TradingEngine)

set(CMAKE_CXX_FLAGS "-std=c++17 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(BOOST_MIN_VERSION "1.70.0")

find_package(Boost REQUIRED COMPONENTS iostreams thread)

file(GLOB server_src src/server/*.cpp)

file(GLOB server_inc
    ${PROJECT_SOURCE_DIR}/include/server
    ${PROJECT_SOURCE_DIR}/include/server/matching
    ${PROJECT_SOURCE_DIR}/include/server/order
    ${PROJECT_SOURCE_DIR}/include/util
    ${PROJECT_SOURCE_DIR}/include/info
    ${PROJECT_SOURCE_DIR}/include/server/rpc
)

find_package(Protobuf CONFIG)
if (${Protobuf_FOUND})
    set(protobuf_MODULE_COMPATIBLE TRUE)
    set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
    set(_REFLECTION gRPC::grpc++_reflection)
    if(CMAKE_CROSSCOMPILING)
        find_program(_PROTOBUF_PROTOC protoc)
    else()
        set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
    endif()
    find_package(gRPC CONFIG REQUIRED)
    message(STATUS "Using gRPC ${gRPC_VERSION}")
    set(_GRPC_GRPCPP gRPC::grpc++)
    if (CMAKE_CROSSCOMPILING)
        find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    else ()
        set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
    endif ()
else (${Protobuf_FOUND})
    message(STATUS "Could not find gRPC, fetching content. This might take a while")
    include(FetchContent)
    FetchContent_Declare(
        grpc
        GIT_REPOSITORY https://github.com/grpc/grpc.git
        GIT_TAG v1.38.0
    )
    FetchContent_MakeAvailable(grpc)
    set(_PROTOBUF_LIBPROTOBUF libprotobuf)
    set(_REFLECTION grpc++ reflection)
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
    set(_GRPC_GRPCPP grpc++)
    if(CMAKE_CROSSCOMPILING)
        find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    else()
        set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
    endif()
endif (${Protobuf_FOUND})

get_filename_component(oe_proto "src/dataprovider/orderentry.proto" ABSOLUTE)
get_filename_component(oe_proto_path "${oe_proto}" PATH)

set(oe_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/orderentry.pb.cc")
set(oe_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/orderentry.pb.h")
set(oe_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/orderentry.grpc.pb.cc")
set(oe_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/orderentry.grpc.pb.h")
add_custom_command(
    OUTPUT "${oe_proto_srcs}" "${oe_proto_hdrs}" "${oe_grpc_srcs}" "${oe_grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${oe_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${oe_proto}"
    DEPENDS "${oe_proto}"
)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(oe_grpc_proto
    ${oe_grpc_srcs}
    ${oe_grpc_hdrs}
    ${oe_proto_srcs}
    ${oe_proto_hdrs}
)
target_link_libraries(oe_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
)

add_executable(tradeserver ${server_src})
target_include_directories(tradeserver PUBLIC ${server_inc})
target_link_libraries(tradeserver 
    ${Boost_LIBRARIES} 
    oe_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
)

include_directories(${Boost_INCLUDE_DIR} include/)
