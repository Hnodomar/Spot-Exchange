cmake_minimum_required(VERSION 3.16)

include(FetchContent)
find_package(Catch2 2)
if (${Catch2_FOUND})
    message(STATUS "Catch2 found")
else(${Catch2_FOUND})
    message(STATUS "Catch2 not found, fetching content")
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v2.13.7
    )
    FetchContent_MakeAvailable(Catch2)
endif(${Catch2_FOUND})
add_executable(orderbook_test 
    orderbooktest.cpp
    ${PROJECT_SOURCE_DIR}/src/server/order/orderbookmanager.cpp
    ${PROJECT_SOURCE_DIR}/src/server/order/orderbook.cpp
    ${PROJECT_SOURCE_DIR}/src/server/matching/fifomatching.cpp
)
target_link_libraries(orderbook_test PRIVATE Catch2::Catch2)
target_include_directories(orderbook_test PRIVATE ${server_inc})

include(CTest)
include(Catch)
catch_discover_tests(orderbook_test)
