#include "client.hpp"

using namespace client;

int main() {
    TradingClient client(
        grpc::CreateChannel(
            "127.0.0.1:9001",
            grpc::InsecureChannelCredentials()
        )
    );
    client.startOrderEntry();
}
