#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "orderbookmanager.hpp"

using namespace server::tradeorder;
using namespace ::tradeorder;

class OrderEntryStreamConnection {}; // test class
using Conn = OrderEntryStreamConnection;

TEST_CASE("OrderBook Operations") {
    logging::Logger logger;
    OrderBookManager test_manager(nullptr);
    OrderEntryStreamConnection connobj;
    OrderEntryStreamConnection* conn = &connobj;
    SECTION("Add Order") {
        uint8_t tickerarr[8] = {'T', 'e', 's', 't', 0, 0, 0, 0};
        uint64_t ticker = *reinterpret_cast<uint64_t*>(tickerarr);
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
        tradeorder::Order test_order_one(1, conn, 100, 2000, info::OrderCommon(10, 1, ticker));
        test_manager.addOrder(test_order_one);
        REQUIRE(test_book.numLevels() == 1);
        REQUIRE(test_book.numOrders() == 1);

        // add order two to orderbook one
        Order test_order_two(0, conn, 100, 4000, info::OrderCommon(2, 2, ticker));
        test_manager.addOrder(test_order_two);
        REQUIRE(test_book.numLevels() == 1);
        REQUIRE(test_book.numOrders() == 1);
        REQUIRE(test_order_two.getCurrQty() == 2000);
        
        // add order three to orderbook one
        Order test_order_three(1, conn, 150, 100, info::OrderCommon(100, 200, ticker));
        test_manager.addOrder(test_order_three);
        auto getorder_res = test_book.getOrder(2);
        REQUIRE(getorder_res.first);
        auto test_order_two_currently = getorder_res.second;
        REQUIRE(test_order_two_currently.getCurrQty() == 1900);
        REQUIRE(test_book.numLevels() == 1);
        REQUIRE(test_book.numOrders() == 1);
        REQUIRE(test_order_three.getCurrQty() == 0);

        // add order four to orderbook two
        Order test_order_four (1, conn, 500, 500, info::OrderCommon(12, 10, util::convertStrToEightBytes("TestTwo")));
        test_manager.addOrder(test_order_four);

        // check order numbers and level numbers
        REQUIRE(test_book.numLevels() == 1);
        REQUIRE(test_book.numOrders() == 1);
        REQUIRE(test_book_two.numLevels() == 1);
        REQUIRE(test_book_two.numOrders() == 1);
    }
    SECTION("Add Many Orders") {
        REQUIRE(test_manager.createOrderBook("AddManyOrders") == true);
        uint64_t ticker = util::convertStrToEightBytes("AddManyOrders");
        auto sub_result = test_manager.subscribe("AddManyOrders");
        REQUIRE(sub_result.first == true);
        OrderBook& orderbook = sub_result.second;
        for (int i = 0; i < 200; ++i) {
            if (i % 2 == 0) {
                Order testorder(1, conn, 100 + i, 100, info::OrderCommon(i, i, ticker));
                test_manager.addOrder(testorder);
            }
            else {
                Order testorder(0, conn, 300 + i, 100, info::OrderCommon(i, i, ticker));
                test_manager.addOrder(testorder);
            }
        }
        REQUIRE(orderbook.numOrders() == 200);
        REQUIRE(orderbook.numLevels() == 200);
    }
    SECTION("Add Many Matching Orders") {
        uint64_t ticker = util::convertStrToEightBytes("ManyMatchingOrders");
        REQUIRE(test_manager.createOrderBook(ticker) == true);
        auto sub_result = test_manager.subscribe(ticker);
        REQUIRE(sub_result.first == true);
        OrderBook& orderbook = sub_result.second;
        for (int i = 0; i < 200; ++i) {
            if (i % 2 == 0) {
                Order testorder(1, conn, 100 + i, 10000, info::OrderCommon(i, i, ticker));
                test_manager.addOrder(testorder);
                REQUIRE((testorder.getCurrQty() == 10000 || testorder.getCurrQty() == 9000));
            }
            else {
                Order testorder(0, conn, 100 + i, 1000, info::OrderCommon(i, i, ticker));
                test_manager.addOrder(testorder);
                REQUIRE(testorder.getCurrQty() == 1000);
            }
        }
        REQUIRE(orderbook.numOrders() == 101);
        REQUIRE(orderbook.numLevels() == 101);
    }
    SECTION("Add Order Non-existent Orderbook") {
        uint64_t ticker = util::convertStrToEightBytes("AddNonExistent");
        REQUIRE(test_manager.createOrderBook(ticker) == true);
        auto sub_result = test_manager.subscribe(ticker);
        REQUIRE(sub_result.first == true);
        OrderBook& orderbook = sub_result.second;
        Order test_order(1, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        Order match_order(0, conn, 100, 100, info::OrderCommon(2, 2, 22));
        test_manager.addOrder(test_order);
        test_manager.addOrder(match_order);
        REQUIRE(match_order.getCurrQty() == 100);
        REQUIRE(orderbook.numLevels() == 1);
        REQUIRE(orderbook.numOrders() == 1);
    }
    SECTION("Add Order ID Already Present") {
        uint64_t ticker = util::convertStrToEightBytes("AddPresent");
        REQUIRE(test_manager.createOrderBook(ticker) == true);
        auto sub_result = test_manager.subscribe(ticker);
        REQUIRE(sub_result.first == true);
        OrderBook& orderbook = sub_result.second;
        Order test_order(1, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        Order match_order(0, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        test_manager.addOrder(test_order);
        test_manager.addOrder(match_order);
        REQUIRE(match_order.getCurrQty() == 100);
        REQUIRE(orderbook.numLevels() == 1);
        REQUIRE(orderbook.numOrders() == 1);    
    }
    SECTION("Modify Order Price no Fills") {
        uint64_t ticker = util::convertStrToEightBytes("ModNoFills");
        test_manager.createOrderBook(ticker);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        info::ModifyOrder morder(1, conn, 350, 100, info::OrderCommon(1, 1, ticker));
        Order order_one(1, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        Order order_two(0, conn, 500, 100, info::OrderCommon(2, 2, ticker));
        test_manager.addOrder(order_one);
        test_manager.addOrder(order_two);
        REQUIRE(orderbook.numLevels() == 2);
        REQUIRE(orderbook.numOrders() == 2);
        test_manager.modifyOrder(morder);
        REQUIRE(orderbook.numLevels() == 2);
        REQUIRE(orderbook.numOrders() == 2);
        REQUIRE(morder.quantity == 100);
    }
    SECTION("Modify Order Price with Fills") {
        uint64_t ticker = util::convertStrToEightBytes("ModFills");
        test_manager.createOrderBook(ticker);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        info::ModifyOrder morder(1, conn, 350, 100, info::OrderCommon(1, 1, ticker));
        Order order_one(1, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        Order order_two(0, conn, 350, 150, info::OrderCommon(2, 2, ticker));
        test_manager.addOrder(order_one);
        test_manager.addOrder(order_two);
        REQUIRE(orderbook.numOrders() == 2);
        REQUIRE(orderbook.numLevels() == 2);
        test_manager.modifyOrder(morder);
        REQUIRE(orderbook.numLevels() == 1);
        REQUIRE(orderbook.numOrders() == 1);
    }
    SECTION("Modify Order Quantity") {
        uint64_t ticker = util::convertStrToEightBytes("modQty");
        test_manager.createOrderBook(ticker);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        Order test_order(1, conn, 2000, 100, info::OrderCommon(1, 1, ticker));
        test_manager.addOrder(test_order);
        auto getorig_res = orderbook.getOrder(1);
        REQUIRE(getorig_res.first);
        auto orig_order = getorig_res.second;
        REQUIRE(orig_order.getCurrQty() == 100);
        REQUIRE(orig_order.getPrice() == 2000);
        info::ModifyOrder morder(1, conn, 100, 200, info::OrderCommon(1, 1, ticker));
        test_manager.modifyOrder(morder);
        auto getmod_res = orderbook.getOrder(1);
        REQUIRE(getmod_res.first);
        auto mod_order = getmod_res.second;
        REQUIRE(mod_order.getPrice() == 100);
        REQUIRE(mod_order.getCurrQty() == 200);
        REQUIRE(orderbook.numLevels() == 1);
        REQUIRE(orderbook.numOrders() == 1);
    }
    SECTION("Modify Order Non-Existent Order") {
        uint64_t ticker = util::convertStrToEightBytes("ModNonExistent");
        test_manager.createOrderBook(ticker);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        info::ModifyOrder morder(1, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        test_manager.modifyOrder(morder);
        REQUIRE(orderbook.numLevels() == 0);
        REQUIRE(orderbook.numOrders() == 0);
    }
    SECTION("Modify Wrong Side") {
        uint64_t ticker = util::convertStrToEightBytes("ModWrongSide");
        test_manager.createOrderBook(ticker);
        info::ModifyOrder morder(0, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        Order test_order(1, conn, 200, 200, info::OrderCommon(1, 1, ticker));
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        test_manager.addOrder(test_order);
        test_manager.modifyOrder(morder);
        auto getorder_res = orderbook.getOrder(1);
        REQUIRE(getorder_res.first);
        auto orig_order = getorder_res.second;
        REQUIRE(orig_order.getCurrQty() == 200);
        REQUIRE(orig_order.getPrice() == 200);
        REQUIRE(orderbook.numOrders() == 1);
        REQUIRE(orderbook.numLevels() == 1);
    }
    SECTION("Modify Other User's Order") {
        uint64_t ticker = util::convertStrToEightBytes("ModOther");
        test_manager.createOrderBook(ticker);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        info::ModifyOrder morder(1, conn, 200, 150, info::OrderCommon(1, 2, ticker));
        Order test_order(1, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        test_manager.addOrder(test_order);
        test_manager.modifyOrder(morder);
        auto getorder_res = orderbook.getOrder(1);
        REQUIRE(getorder_res.first);
        auto orig_order = getorder_res.second;
        REQUIRE(orig_order.getCurrQty() == 100);
        REQUIRE(orig_order.getPrice() == 100);
        REQUIRE(orderbook.numLevels() == 1);
        REQUIRE(orderbook.numOrders() == 1);
    }
    SECTION("Cancel Only Order Left in Level") {
        uint64_t ticker = util::convertStrToEightBytes("CancelOne");
        test_manager.createOrderBook(ticker);
        Order test_order(1, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        test_manager.addOrder(test_order);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        REQUIRE(orderbook.numOrders() == 1);
        REQUIRE(orderbook.numLevels() == 1);
        info::CancelOrder cancel_order(1, 1, ticker, conn);
        test_manager.cancelOrder(cancel_order);
        REQUIRE(orderbook.numOrders() == 0);
        REQUIRE(orderbook.numLevels() == 0);
    }
    SECTION("Cancel All of Two Orders in Level") {
        uint64_t ticker = util::convertStrToEightBytes("CancelTwo");
        test_manager.createOrderBook(ticker);
        Order order_one(1, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        Order order_two(1, conn, 100, 100, info::OrderCommon(2, 2, ticker));
        test_manager.addOrder(order_one);
        test_manager.addOrder(order_two);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        REQUIRE(orderbook.numOrders() == 2);
        REQUIRE(orderbook.numLevels() == 1);
        info::CancelOrder cancel_order_one(1, 1, ticker, conn);
        info::CancelOrder cancel_order_two(2, 2, ticker, conn);
        test_manager.cancelOrder(cancel_order_one);
        REQUIRE(orderbook.numOrders() == 1);
        REQUIRE(orderbook.numLevels() == 1);
        test_manager.cancelOrder(cancel_order_two);
        REQUIRE(orderbook.numOrders() == 0);
        REQUIRE(orderbook.numLevels() == 0);
    }
    SECTION("Cancel Orders in Middle of Queue in Level") {
        uint64_t ticker = util::convertStrToEightBytes("CancelFive");
        test_manager.createOrderBook(ticker);
        info::CancelOrder cancel_order_three(3, 3, ticker, conn);
        info::CancelOrder cancel_order_four(4, 4, ticker, conn);
        Order order_one(1, conn, 100, 100, info::OrderCommon(1, 1, ticker));
        Order order_two(1, conn, 100, 100, info::OrderCommon(2, 2, ticker));
        Order order_three(1, conn, 100, 100, info::OrderCommon(3, 3, ticker));
        Order order_four(1, conn, 100, 100, info::OrderCommon(4, 4, ticker));
        Order order_five(1, conn, 100, 100, info::OrderCommon(5, 5, ticker));
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
        test_manager.cancelOrder(cancel_order_three);
        REQUIRE(orderbook.numOrders() == 4);
        test_manager.cancelOrder(cancel_order_four);
        REQUIRE(orderbook.numOrders() == 3);
    }
    SECTION("Cancel Order that Doesn't Exist") { 
        uint64_t ticker = util::convertStrToEightBytes("CancelNonExist");
        test_manager.createOrderBook(ticker);
        info::CancelOrder cancel_order(1, 1, ticker, conn);
        REQUIRE_NOTHROW(test_manager.cancelOrder(cancel_order));
    }
    SECTION("Cancel Someone Else's Order") {
        uint64_t ticker = util::convertStrToEightBytes("CancelOther");
        test_manager.createOrderBook(ticker);
        auto sub_res = test_manager.subscribe(ticker);
        REQUIRE(sub_res.first);
        auto& orderbook = sub_res.second;
        Order test_order(1, conn, 100, 100, info::OrderCommon(2, 2, ticker));
        test_manager.addOrder(test_order);
        info::CancelOrder cancel_order(2, 1, ticker, conn);
        test_manager.cancelOrder(cancel_order);
        REQUIRE(orderbook.numOrders() == 1);
        REQUIRE(orderbook.numLevels() == 1);
    }
}
