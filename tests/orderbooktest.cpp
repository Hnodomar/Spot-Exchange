#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "orderbookmanager.hpp"

using namespace server::tradeorder;

void testManyMatchedOrdersResult(OrderResult& res, const uint expected_fills) {
    if (res.match_result != std::nullopt) {
        if (res.match_result->orderCompletelyFilled()) {
            REQUIRE(res.order_status_present == info::OrderStatusPresent::NoStatusPresent);
            REQUIRE(res.match_result->numFills() == expected_fills);
        }
        else 
            REQUIRE(res.order_status_present == info::OrderStatusPresent::NewOrderPresent);
    }
    else 
        REQUIRE(res.order_status_present == info::OrderStatusPresent::NewOrderPresent);
}

void testAddOrderStatus(const info::NewOrderStatus& new_order_stat, const Order& order) {
    REQUIRE(new_order_stat.is_buy_side == order.isBuySide());
    REQUIRE(new_order_stat.order_id == order.getOrderID());
    REQUIRE(new_order_stat.price == order.getPrice());
    REQUIRE(new_order_stat.quantity == order.getCurrQty());
    REQUIRE(new_order_stat.ticker == order.getTicker());
    REQUIRE(new_order_stat.user_id == order.getUserID());
}

TEST_CASE("OrderBook Operations") {
    uint8_t tickerarr[8] = {'T', 'e', 's', 't', 0, 0, 0, 0};
    uint64_t ticker = *reinterpret_cast<uint64_t*>(tickerarr);
    OrderBookManager test_manager;
    SECTION("Add Order") {
        // create orderbook one
        REQUIRE(test_manager.createOrderBook("Test") == true); // test str to uint64_t conversion
        auto sub_result = test_manager.subscribe(ticker);
        REQUIRE(sub_result.first == true);
        OrderBook& test_book = sub_result.second;

        // create orderbook two
        test_manager.createOrderBook("TestTwo");
        auto sub_result_two = test_manager.subscribe(ticker);
        assert(sub_result_two.first);
        OrderBook& test_book_two = sub_result_two.second;

        // add order one to orderbook one
        Order test_order_one(1, 100, 2000, info::OrderCommon(10, 1, ticker));
        OrderResult first_add_result = test_manager.addOrder(test_order_one);
        REQUIRE(first_add_result.match_result == std::nullopt);
        REQUIRE(first_add_result.order_status_present == info::OrderStatusPresent::NewOrderPresent);
        testAddOrderStatus(first_add_result.orderstatus.new_order_status, test_order_one);

        // add order two to orderbook one
        Order test_order_two(0, 100, 4000, info::OrderCommon(2, 2, ticker));
        OrderResult second_add_result = test_manager.addOrder(test_order_two);
        REQUIRE(second_add_result.match_result != std::nullopt);
        REQUIRE(second_add_result.order_status_present == info::OrderStatusPresent::NewOrderPresent);
        REQUIRE(second_add_result.orderstatus.new_order_status.quantity == 2000);
        
        // add order three to orderbook one
        Order test_order_three(1, 150, 100, info::OrderCommon(100, 200, ticker));
        OrderResult third_add_result = test_manager.addOrder(test_order_three);
        REQUIRE(third_add_result.match_result != std::nullopt);
        REQUIRE(third_add_result.order_status_present == info::OrderStatusPresent::NoStatusPresent);

        // add order four to orderbook two
        Order test_order_four (1, 500, 500, info::OrderCommon(12, 10, util::convertStrToEightBytes("TestTwo")));
        OrderResult fourth_add_result = test_manager.addOrder(test_order_four);
        REQUIRE(fourth_add_result.match_result == std::nullopt);
        REQUIRE(fourth_add_result.order_status_present == info::OrderStatusPresent::NewOrderPresent);

        // check order numbers and level numbers
        REQUIRE(test_book.numLevels() == 1);
        REQUIRE(test_book.numOrders() == 1);
        REQUIRE(test_book_two.numLevels() == 1);
        REQUIRE(test_book_two.numOrders() == 1);
    }
    SECTION("Add Many Orders") {
        REQUIRE(test_manager.createOrderBook("Test") == true);
        auto sub_result = test_manager.subscribe(ticker);
        REQUIRE(sub_result.first == true);
        OrderBook& orderbook = sub_result.second;
        for (int i = 0; i < 200; ++i) {
            if (i % 2 == 0) {
                Order testorder(1, 100 + i, 100, info::OrderCommon(i, i, ticker));
                test_manager.addOrder(testorder);
            }
            else {
                Order testorder(0, 300 + i, 100, info::OrderCommon(i, i, ticker));
                test_manager.addOrder(testorder);
            }
        }
        REQUIRE(orderbook.numOrders() == 200);
        REQUIRE(orderbook.numLevels() == 200);
    }
    SECTION("Add Many Matching Orders") {
        REQUIRE(test_manager.createOrderBook("Test") == true);
        auto sub_result = test_manager.subscribe(ticker);
        REQUIRE(sub_result.first == true);
        OrderBook& orderbook = sub_result.second;
        for (int i = 0; i < 200; ++i) {
            if (i % 2 == 0) {
                Order testorder(1, 100 + i, 10000, info::OrderCommon(i, i, ticker));
                auto res = test_manager.addOrder(testorder);
                testManyMatchedOrdersResult(res, 10);
            }
            else {
                Order testorder(0, 100 + i, 1000, info::OrderCommon(i, i, ticker));
                auto res = test_manager.addOrder(testorder);
                testManyMatchedOrdersResult(res, 2);
            }
        }
        REQUIRE(orderbook.numOrders() == 101);
        REQUIRE(orderbook.numLevels() == 101);
    }
    SECTION("Add Order Non-existent Orderbook") {
        Order test_order(1, 100, 100, info::OrderCommon(1, 1, ticker));
        OrderResult order_result = test_manager.addOrder(test_order);
        REQUIRE(order_result.order_status_present == info::OrderStatusPresent::RejectionPresent);
        REQUIRE(order_result.orderstatus.rejection == info::RejectionReason::orderbook_not_found);
    }
    SECTION("Add Order ID Already Present") {
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
    SECTION("Modify Order Price no Fills") {
        test_manager.createOrderBook(ticker);
        info::ModifyOrder morder(1, 350, 100, info::OrderCommon(1, 1, ticker));
        Order order_one(1, 100, 100, info::OrderCommon(1, 1, ticker));
        Order order_two(0, 500, 100, info::OrderCommon(2, 2, ticker));
        test_manager.addOrder(order_one);
        test_manager.addOrder(order_two);
        auto res = test_manager.modifyOrder(morder);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        REQUIRE(orderbook.numLevels() == 2);
        REQUIRE(orderbook.numOrders() == 2);
        REQUIRE(!res.second.empty());
        REQUIRE(res.second.order_status_present == info::OrderStatusPresent::CancelOrderPresent);
        REQUIRE(res.second.match_result == std::nullopt);
        REQUIRE(res.first.order_status_present == info::OrderStatusPresent::NewOrderPresent);
        REQUIRE(res.first.orderstatus.new_order_status.price == 350);
        REQUIRE(res.first.orderstatus.new_order_status.quantity == 100);
        REQUIRE(res.first.match_result->numFills() == 0);
    }
    SECTION("Modify Order Price with Fills") {
        test_manager.createOrderBook(ticker);
        info::ModifyOrder morder(1, 350, 100, info::OrderCommon(1, 1, ticker));
        Order order_one(1, 100, 100, info::OrderCommon(1, 1, ticker));
        Order order_two(0, 350, 150, info::OrderCommon(2, 2, ticker));
        test_manager.addOrder(order_one);
        test_manager.addOrder(order_two);
        auto res = test_manager.modifyOrder(morder);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        REQUIRE(orderbook.numLevels() == 1);
        REQUIRE(orderbook.numOrders() == 1);
        REQUIRE(!res.second.empty());
        REQUIRE(res.second.order_status_present == info::OrderStatusPresent::CancelOrderPresent);
        REQUIRE(res.second.match_result == std::nullopt);
        REQUIRE(res.first.order_status_present == info::OrderStatusPresent::NoStatusPresent);
        REQUIRE(res.first.match_result->numFills() == 2);
    }
    SECTION("Modify Order Quantity") {
        test_manager.createOrderBook(ticker);
        info::ModifyOrder morder(1, 100, 200, info::OrderCommon(1, 1, ticker));
        Order test_order(1, 100, 100, info::OrderCommon(1, 1, ticker));
        test_manager.addOrder(test_order);
        auto res = test_manager.modifyOrder(morder);
        REQUIRE(res.second.empty());
        REQUIRE(res.first.order_status_present == info::OrderStatusPresent::ModifyOrderPresent);
        REQUIRE(res.first.orderstatus.modify_order_status.quantity == 200);
        REQUIRE(res.first.orderstatus.modify_order_status.price == 100);
    }
    SECTION("Modify Order Non-Existent Order") {
        test_manager.createOrderBook(ticker);
        info::ModifyOrder morder(1, 100, 100, info::OrderCommon(1, 1, ticker));
        auto res = test_manager.modifyOrder(morder);
        REQUIRE(res.second.empty());
        REQUIRE(res.first.order_status_present == info::OrderStatusPresent::RejectionPresent);
        REQUIRE(res.first.orderstatus.rejection == info::RejectionReason::order_not_found);
    }
    SECTION("Modify Wrong Side") {
        test_manager.createOrderBook(ticker);
        info::ModifyOrder morder(0, 100, 100, info::OrderCommon(1, 1, ticker));
        Order test_order(1, 100, 100, info::OrderCommon(1, 1, ticker));
        test_manager.addOrder(test_order);
        auto res = test_manager.modifyOrder(morder);
        REQUIRE(res.second.empty());
        REQUIRE(res.first.order_status_present == info::OrderStatusPresent::RejectionPresent);
        REQUIRE(res.first.orderstatus.rejection == info::RejectionReason::modify_wrong_side);
    }
    SECTION("Modify Other User's Order") {
        test_manager.createOrderBook(ticker);
        info::ModifyOrder morder(1, 200, 100, info::OrderCommon(1, 2, ticker));
        Order test_order(1, 100, 100, info::OrderCommon(1, 1, ticker));
        test_manager.addOrder(test_order);
        auto res = test_manager.modifyOrder(morder);
        REQUIRE(res.second.empty());
        REQUIRE(res.first.order_status_present == info::OrderStatusPresent::RejectionPresent);
        REQUIRE(res.first.orderstatus.rejection == info::RejectionReason::wrong_user_id);
    }
    SECTION("Modification Trivial") {
        test_manager.createOrderBook(ticker);
        info::ModifyOrder morder(1, 100, 100, info::OrderCommon(1, 1, ticker));
        Order test_order(1, 100, 100, info::OrderCommon(1, 1, ticker));
        test_manager.addOrder(test_order);
        auto res = test_manager.modifyOrder(morder);
        REQUIRE(res.second.empty());
        REQUIRE(res.first.order_status_present == info::OrderStatusPresent::RejectionPresent);
        REQUIRE(res.first.orderstatus.rejection == info::RejectionReason::modification_trivial);
    }
    SECTION("Cancel Only Order Left in Level") {
        test_manager.createOrderBook(ticker);
        Order test_order(1, 100, 100, info::OrderCommon(1, 1, ticker));
        test_manager.addOrder(test_order);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        REQUIRE(orderbook.numOrders() == 1);
        REQUIRE(orderbook.numLevels() == 1);
        info::CancelOrder cancel_order(1, 1, ticker);
        test_manager.cancelOrder(cancel_order);
        REQUIRE(orderbook.numOrders() == 0);
        REQUIRE(orderbook.numLevels() == 0);
    }
    SECTION("Cancel All of Two Orders in Level") {
        test_manager.createOrderBook(ticker);
        Order order_one(1, 100, 100, info::OrderCommon(1, 1, ticker));
        Order order_two(1, 100, 100, info::OrderCommon(2, 2, ticker));
        test_manager.addOrder(order_one);
        test_manager.addOrder(order_two);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        REQUIRE(orderbook.numOrders() == 2);
        REQUIRE(orderbook.numLevels() == 1);
        info::CancelOrder cancel_order_one(1, 1, ticker);
        info::CancelOrder cancel_order_two(2, 2, ticker);
        test_manager.cancelOrder(cancel_order_one);
        test_manager.cancelOrder(cancel_order_two);
        REQUIRE(orderbook.numOrders() == 0);
        REQUIRE(orderbook.numLevels() == 0);
    }
    SECTION("Cancel Orders in Middle of Queue in Level") {
        test_manager.createOrderBook(ticker);
        info::CancelOrder cancel_order_three(3, 3, ticker);
        info::CancelOrder cancel_order_four(4, 4, ticker);
        Order order_one(1, 100, 100, info::OrderCommon(1, 1, ticker));
        Order order_two(1, 100, 100, info::OrderCommon(2, 2, ticker));
        Order order_three(1, 100, 100, info::OrderCommon(3, 3, ticker));
        Order order_four(1, 100, 100, info::OrderCommon(4, 4, ticker));
        Order order_five(1, 100, 100, info::OrderCommon(5, 5, ticker));
        test_manager.addOrder(order_one);
        test_manager.addOrder(order_two);
        test_manager.addOrder(order_three);
        test_manager.addOrder(order_four);
        test_manager.addOrder(order_five);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        REQUIRE(orderbook.numLevels() == 1);
        REQUIRE(orderbook.numOrders() == 5);
        OrderResult res_three = test_manager.cancelOrder(cancel_order_three);
        REQUIRE(res_three.order_status_present == info::OrderStatusPresent::CancelOrderPresent);
        REQUIRE(res_three.orderstatus.cancel_order_status.order_id == 3);
        REQUIRE(res_three.orderstatus.cancel_order_status.user_id == 3);
        REQUIRE(res_three.orderstatus.cancel_order_status.ticker == ticker);
        REQUIRE(orderbook.numOrders() == 4);
        OrderResult res_four = test_manager.cancelOrder(cancel_order_four);
        REQUIRE(res_four.order_status_present == info::OrderStatusPresent::CancelOrderPresent);
        REQUIRE(res_four.orderstatus.cancel_order_status.order_id == 4);
        REQUIRE(res_four.orderstatus.cancel_order_status.user_id == 4);
        REQUIRE(res_four.orderstatus.cancel_order_status.ticker == ticker);
        REQUIRE(orderbook.numOrders() == 3);
    }
    SECTION("Cancel Order that Doesn't Exist") {
        test_manager.createOrderBook(ticker);
        info::CancelOrder cancel_order(1, 1, ticker);
        OrderResult res = test_manager.cancelOrder(cancel_order);
        REQUIRE(res.order_status_present == info::OrderStatusPresent::RejectionPresent);
        REQUIRE(res.orderstatus.rejection == info::RejectionReason::order_not_found);
    }
    SECTION("Cancel Someone Else's Order") {
        test_manager.createOrderBook(ticker);
        Order test_order(1, 100, 100, info::OrderCommon(2, 2, ticker));
        test_manager.addOrder(test_order);
        info::CancelOrder cancel_order(2, 1, ticker);
        OrderResult res = test_manager.cancelOrder(cancel_order);
        REQUIRE(res.order_status_present == info::OrderStatusPresent::RejectionPresent);
        REQUIRE(res.orderstatus.rejection == info::RejectionReason::wrong_user_id);
    }
}
