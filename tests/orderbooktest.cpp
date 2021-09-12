#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "orderbookmanager.hpp"

using namespace server::tradeorder;

TEST_CASE("OrderBook Operations") {
    uint8_t tickerarr[8] = {'T', 'e', 's', 't', 0, 0, 0, 0};
    uint64_t ticker = *reinterpret_cast<uint64_t*>(tickerarr);
    SECTION("Add Order") {
        OrderBookManager test_manager;
        REQUIRE(test_manager.createOrderBook("Test") == true); // test str to uint64_t conversion
        auto sub_result = test_manager.subscribe(ticker);
        REQUIRE(sub_result.first == true);
        OrderBook& orderbook = sub_result.second;
        Order test_order_one(1, 100, 100, info::OrderCommon(1, 1, ticker));
        Order test_order_two(0, 100, 100, info::OrderCommon(2, 2, ticker));
        Order test_order_three(1, 100, 100, info::OrderCommon(3, 3, ticker));
        OrderResult first_add_result = test_manager.addOrder(test_order_one);
        REQUIRE(first_add_result.match_result == std::nullopt);
        REQUIRE(first_add_result.order_status_present == info::OrderStatusPresent::NewOrderPresent);
        //REQUIRE(first_add_result.orderstatus.)
        OrderResult second_add_result = test_manager.addOrder(test_order_two);
        REQUIRE(second_add_result.match_result != std::nullopt);
        OrderResult third_add_result = test_manager.addOrder(test_order_three);
        REQUIRE(third_add_result.match_result == std::nullopt);
    }
    SECTION("Add Order Non-existent Orderbook") {
        OrderBookManager test_manager;
        Order test_order(1, 100, 100, info::OrderCommon(1, 1, ticker));
        OrderResult order_result = test_manager.addOrder(test_order);
        REQUIRE(order_result.order_status_present == info::OrderStatusPresent::RejectionPresent);
        REQUIRE(order_result.orderstatus.rejection == info::RejectionReason::orderbook_not_found);
    }
    SECTION("Add Order ID Already Present") {
        OrderBookManager test_manager;
        REQUIRE(test_manager.createOrderBook("Test") == true); 
        Order original_order(1, 100, 100, info::OrderCommon(1, 1, ticker));
        Order duplicate_order(1, 100, 100, info::OrderCommon(1, 1, ticker));
        REQUIRE(test_manager.addOrder(
            original_order).order_status_present == info::OrderStatusPresent::NewOrderPresent
        );
        OrderResult result = test_manager.addOrder(duplicate_order);
        REQUIRE(result.order_status_present == info::OrderStatusPresent::RejectionPresent);
        REQUIRE(result.orderstatus.rejection == info::RejectionReason::order_id_already_present);
    }
    SECTION("Modify Order") {
        
    }
    SECTION("Cancel Order") {

    }
}
