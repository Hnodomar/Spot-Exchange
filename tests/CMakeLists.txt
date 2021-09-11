cmake_minimum_required(VERSION 3.16)

include(FetchContent)
find_package(Catch2 3)
if (!${Catch2_FOUND})
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v2.13.7
    )
    FetchContent_MakeAvailable(Catch2)

add_executable(orderbook_test orderbooktest.cpp)
target_link_libraries(orderbook_test PRIVATE Catch2::Catch2WithMain)
target_include_directories(${PROJECT_SOURCE_DIR}/include)
