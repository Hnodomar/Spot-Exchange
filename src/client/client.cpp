#include "client.hpp"

using namespace client;

TradingClient::TradingClient(std::shared_ptr<grpc::Channel> channel)
  : stub_(orderentry::OrderEntryService::NewStub(channel)) 
{}

void TradingClient::startOrderEntry() {
    grpc::ClientContext context;
    std::shared_ptr<
        grpc::ClientReaderWriter<OERequest, OEResponse>
    >oe_stream(stub_->OrderEntry(&context));
    std::thread oe_writer([oe_stream]() {
        for(;;) {

        }
        // take input
    });
    OEResponse oe_response;
    while(oe_stream->Read(&oe_response)) {

    }
    oe_writer.join();
    grpc::Status status = oe_stream->Finish();
    if (!status.ok()) {
        std::cout << "Order Entry RPC failed" << std::endl;
    }
}

void TradingClient::getUserInput() {
    
}
