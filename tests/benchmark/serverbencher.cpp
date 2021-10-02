#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <vector>
#include <random>
#include <string>
#include <chrono>
#include <thread>
#include <iostream>

#include "orderentry.grpc.pb.h"

using OERequest = orderentry::OrderEntryRequest;
using OEResponse = orderentry::OrderEntryResponse;
using Stub = orderentry::OrderEntryService::Stub;
using Writer = std::shared_ptr<grpc::ClientReaderWriter<OERequest, OEResponse>>;

constexpr uint64_t NUM_ORDERS = 10000;
uint64_t USER_ID = 0;
constexpr uint8_t BID_SIDE = 1;
constexpr uint8_t ASK_SIDE = 1;

std::random_device dev;
std::mt19937 rng(dev());
std::uniform_int_distribution<std::mt19937::result_type> dist10(1, 10);
std::uniform_int_distribution<std::mt19937::result_type> dist100(1, 100);

OERequest makeAddOrder(bool is_buy_side, uint64_t price, uint32_t quantity, uint64_t order_id, uint64_t user_id, uint64_t ticker) {
    OERequest temp;
    auto add_order = temp.mutable_new_order();
    add_order->set_is_buy_side(is_buy_side);
    add_order->set_price(price);
    add_order->set_quantity(quantity);
    auto common = add_order->mutable_order_common();
    common->set_order_id(order_id);
    common->set_user_id(USER_ID);
    common->set_ticker(ticker);
    return temp;
}

OERequest makeModifyOrder(bool is_buy_side, uint64_t price, uint32_t quantity, uint64_t order_id, uint64_t user_id, uint64_t ticker) {
    OERequest temp;
    auto mod_order = temp.mutable_modify_order();
    mod_order->set_is_buy_side(is_buy_side);
    mod_order->set_price(price);
    mod_order->set_quantity(quantity);    
    auto common = mod_order->mutable_order_common();
    common->set_order_id(order_id);
    common->set_user_id(USER_ID);
    common->set_ticker(ticker);    
    return temp;
}

OERequest makeCancelOrder(uint64_t order_id, uint64_t user_id, uint64_t ticker) {
    OERequest temp;
    auto cancel = temp.mutable_cancel_order();
    auto common = cancel->mutable_order_common();
    common->set_order_id(order_id);
    common->set_user_id(USER_ID);
    common->set_ticker(ticker);    
    return temp;
}

uint64_t getNewOrderAckID(const OEResponse& resp) {
    return resp.new_order_ack().new_order().order_common().order_id();
}

std::vector<OERequest> setupCancelOrders(Writer& writer) {
    std::vector<OERequest> cancels;
    OEResponse resp;
    for (uint64_t i = 0; i < NUM_ORDERS / 2; ++i) {
        uint64_t instrument = dist100(rng);
        if (i < (NUM_ORDERS / 5)) { // 20% of cancels will result in level data-structure erasure
            writer->Write(makeAddOrder(ASK_SIDE, i + 160, 100, 1, USER_ID, instrument));
            writer->Read(&resp);
            cancels.push_back(makeCancelOrder(getNewOrderAckID(resp), USER_ID, instrument));
            writer->Write(makeAddOrder(ASK_SIDE, i + 160, 100, 1, USER_ID, instrument));
            writer->Read(&resp);
            cancels.push_back(makeCancelOrder(getNewOrderAckID(resp), USER_ID, instrument));
        }
        else {
            int num_iterations = dist10(rng); // want to randomise the position of our orders that must be cancelled
            for (int j = 0; j < num_iterations; ++j) {
                writer->Write(makeAddOrder(BID_SIDE, 50, 100, 1, USER_ID, instrument));
                writer->Read(&resp);
                writer->Write(makeAddOrder(ASK_SIDE, 150, 100, 1, USER_ID, instrument));
                writer->Read(&resp);
            } 

            writer->Write(makeAddOrder(BID_SIDE, 50, 100, 1, USER_ID, instrument));
            writer->Read(&resp);
            cancels.push_back(makeCancelOrder(getNewOrderAckID(resp), USER_ID, instrument));
            writer->Write(makeAddOrder(ASK_SIDE, 150, 100, 1, USER_ID, instrument));
            writer->Read(&resp);
            cancels.push_back(makeCancelOrder(getNewOrderAckID(resp), USER_ID, instrument));

            for (int j = 0; j < 10 - num_iterations; ++j) {
                writer->Write(makeAddOrder(BID_SIDE, 50, 100, 1, USER_ID, instrument));
                writer->Read(&resp);
                writer->Write(makeAddOrder(ASK_SIDE, 150, 100, 1, USER_ID, instrument));
                writer->Read(&resp);
            }
        }
    }
    return cancels;
}

std::vector<OERequest> setupAddOrders(Writer& writer) {
    std::vector<OERequest> adds;
    for (uint64_t i = 0; i < (NUM_ORDERS / 10); ++i) {
        uint64_t instrument = dist100(rng); // send same orders to 100 different instruments
        // NUM_ORDERS / 10 orders of same pattern testing key add order behaviour
        adds.push_back(makeAddOrder(BID_SIDE, 98, 100, 1, USER_ID, instrument));
        adds.push_back(makeAddOrder(ASK_SIDE, 103, 100, 1, USER_ID, instrument));
        adds.push_back(makeAddOrder(BID_SIDE, 100, 100, 1, USER_ID, instrument));
        adds.push_back(makeAddOrder(ASK_SIDE, 100, 50, 1, USER_ID, instrument));
        adds.push_back(makeAddOrder(BID_SIDE, 101, 100, 1, USER_ID, instrument));
        adds.push_back(makeAddOrder(ASK_SIDE, 100, 50, 1, USER_ID, instrument));
        adds.push_back(makeAddOrder(BID_SIDE, 99, 100, 1, USER_ID, instrument));
        adds.push_back(makeAddOrder(ASK_SIDE, 101, 50, 1, USER_ID, instrument));
        adds.push_back(makeAddOrder(BID_SIDE, 102, 100, 1, USER_ID, instrument));
        adds.push_back(makeAddOrder(BID_SIDE, 98, 100, 1, USER_ID, instrument));
    }
    return adds;
}

std::vector<OERequest> setupModifyOrders(Writer& writer) {
    std::vector<OERequest> modifys;
    OEResponse resp;
    for (uint64_t i = 0; i < NUM_ORDERS / 2; ++i) {
        uint64_t instrument = dist100(rng);
        if (i < NUM_ORDERS / 10) {
            writer->Write(makeAddOrder(BID_SIDE, 40, 50, 1, USER_ID, instrument));
            writer->Read(&resp);
            modifys.push_back(makeModifyOrder(BID_SIDE, 40, 200, getNewOrderAckID(resp), USER_ID, instrument));

            writer->Write(makeAddOrder(ASK_SIDE, 140, 50, 1, USER_ID, instrument));
            writer->Read(&resp);
            modifys.push_back(makeModifyOrder(ASK_SIDE, 140, 200, getNewOrderAckID(resp), USER_ID, instrument));
        }
        else {
            writer->Write(makeAddOrder(BID_SIDE, 40, 100, 1, USER_ID, instrument));
            writer->Read(&resp);
            modifys.push_back(makeModifyOrder(BID_SIDE, 30, 80, getNewOrderAckID(resp), USER_ID, instrument));

            writer->Write(makeAddOrder(ASK_SIDE, 140, 100, 1, USER_ID, instrument));
            writer->Read(&resp);
            modifys.push_back(makeModifyOrder(ASK_SIDE, 130, 80, getNewOrderAckID(resp), USER_ID, instrument));
        }
    }
    return modifys;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        return 1;
    }
    USER_ID = std::atoi(argv[1]);
    auto stub(orderentry::OrderEntryService::NewStub(
        grpc::CreateChannel("127.0.0.1:9001", grpc::InsecureChannelCredentials())
    ));
    grpc::ClientContext context;
    std::shared_ptr<
        grpc::ClientReaderWriter<OERequest, OEResponse>
    >oe_stream(stub->OrderEntry(&context));
    auto cancels = setupCancelOrders(oe_stream);
    auto mods = setupModifyOrders(oe_stream);
    auto adds = setupAddOrders(oe_stream);
    for (int i = 0; i < NUM_ORDERS; ++i) {
        oe_stream->Write(cancels[i]);
        oe_stream->Write(mods[i]);
        oe_stream->Write(adds[i]);
    }
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(30s);
}
