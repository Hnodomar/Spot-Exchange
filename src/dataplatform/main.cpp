#include "dataplatform.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3)
        return 1;
    dataplatform::DataPlatform dp(
        grpc::CreateChannel(
            std::string(std::string(argv[1], strlen(argv[1])) + ":" + argv[2]),
            grpc::InsecureChannelCredentials()
        )
    );
    dp.initiateMarketDataStream();
}
