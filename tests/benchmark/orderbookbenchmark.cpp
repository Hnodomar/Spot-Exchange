#include <benchmark/benchmark.h>
#include <vector>
#include <string>
#include <random>

#include "orderbookmanager.hpp"

using namespace server::tradeorder;
using namespace ::tradeorder;

class OrderEntryStreamConnection {};
using Conn = OrderEntryStreamConnection;

std::random_device dev;
std::mt19937 rng(dev());

// 50% add order
// 30% cancel order
// 5% modify
// 15% replace

static void setupOrdersAndBook(OrderBookManager& m, std::vector<Order>& adds ) {
    constexpr int8_t num_instruments = 10;
    for (uint64_t i = 0; i < 100; ++i) {
        m.createOrderBook(i); // 100 instruments
    }
    // evenly distribute 10000 orders amongst instruments
    std::uniform_int_distribution<std::mt19937::result_type> dist6(1, 100);
    for (uint64_t i = 0; i < 100000; ++i) {
        uint64_t instrument = i % 100;
        uint64_t side = i % 2;
        uint64_t qty = dist6(rng);
        uint64_t price = 100 + (i % 100);
        if (side) {
            Order temp(side, nullptr, price, qty, info::OrderCommon(i, i, instrument));
            adds.push_back(temp);
        }
        else {
            Order temp(side, nullptr, price, qty, info::OrderCommon(i, i, instrument));
            adds.push_back(temp);
        }
    }
}

static void BM_OrderBook(benchmark::State& state) {
    OrderBookManager bench_manager(nullptr);
    OrderEntryStreamConnection connobj;
    OrderEntryStreamConnection* conn = &connobj;
    std::vector<tradeorder::Order> adds;
    std::vector<info::ModifyOrder> mods;
    setupOrdersAndBook(bench_manager, adds);
    for (auto arg : state) {
        for (auto& add : adds)
            bench_manager.addOrder(add);
    }
}

BENCHMARK(BM_OrderBook);

BENCHMARK_MAIN();
