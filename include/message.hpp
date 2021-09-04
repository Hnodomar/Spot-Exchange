#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdint>

namespace msg {
constexpr uint8_t HEADER_LEN = 2;

class Message {
public: 
    uint8_t* getMsgHeader() {
        return data_;
    }
    uint8_t* getMsgBody() {
        return data_ + HEADER_LEN;
    }
    uint8_t getBodyLen() {
        return body_len_;
    }
private:
    uint8_t body_len_;
    uint8_t data_[256];  
};
}

#endif
