#ifndef TEMP_BUFFER_HPP
#define TEMP_BUFFER_HPP

#include <cstdint>
#include <cstring>
#include <array>
#include <algorithm>

#include "order.hpp"

using namespace tradeorder;
namespace util {
static constexpr std::array<uint8_t, 4> allowedtypes = {'A', 'M', 'C', 'R'};
struct TempBuffer {
    bool parseHeader() {
        uint8_t header[HEADER_LEN + 1] = {0};
        std::memcpy(header, buffer_, HEADER_LEN);
        length_ = header[0];
        type_ = header[1];
        if (length_ > MAX_BODY_LEN)
            return false;
        if (invalidType(type_))
            return false;
        return true;
    }
    bool invalidType(uint8_t type) {
        auto itr = std::find(
            std::begin(allowedtypes),
            std::end(allowedtypes),
            type
        );
        if (itr == std::end(allowedtypes))
            return false;
        return true;
    }
    uint8_t getBodyLength() const {return length_;}
    uint8_t getOrderType() const {return type_;}
    uint8_t* getBuffer() {return buffer_;}
    uint8_t* getBodyBuffer() {return buffer_ + HEADER_LEN;}
private:
    uint8_t length_;
    uint8_t type_;
    uint8_t buffer_[HEADER_LEN + MAX_BODY_LEN];
};
}

#endif
