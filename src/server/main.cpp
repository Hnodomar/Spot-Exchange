#include "tradeserver.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Call with correct args: [port] [OPTIONAL: log file]" << std::endl;
        return 1;
    }
    server::TradeServer server(argv[1], argv[2]);
    return 0;
}
