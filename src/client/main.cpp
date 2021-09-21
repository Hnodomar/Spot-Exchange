#include "client.hpp"

using namespace client;

int main(int argc, char* argv[]) {
    TradingClient client(
        grpc::CreateChannel(
            "127.0.0.1:9001",
            grpc::InsecureChannelCredentials()
        ),
        argv[1], argv[2]
    );
    client.startOrderEntry();
}
