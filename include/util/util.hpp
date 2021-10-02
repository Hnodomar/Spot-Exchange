#ifndef UTIL_HPP
#define UTIL_HPP

#include <chrono>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <sys/ioctl.h>

// misc utility functions
namespace util {
static inline int64_t getUnixTimestamp() {
    using time = std::chrono::system_clock;
    auto now = time::now();
    int64_t tnow = time::to_time_t(now);
    tm *date = std::localtime(&tnow);
    date->tm_hour = 0;
    date->tm_min = 0;
    date->tm_sec = 0;
    auto midnight = time::from_time_t(std::mktime(date));
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now - midnight).count();
}

static inline std::time_t getUnixEpochTimestamp() {
    return std::time(nullptr);
}

static inline std::string getLogTimestamp() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "[%d/%m/%Y-%H:%M:%S]");
    return oss.str();
}

class ShortString { // avoid std::string SBO - save 24 bytes for 8-byte strings
public:
    ShortString(uint64_t representation) {
        std::memcpy(short_string, &representation, 8);
    }
    friend inline std::ostream& operator<<(std::ostream& o, ShortString& rhs) {
        for (auto i = 0; i < 8; ++i) {
            if (rhs.short_string[i] == '\0')
                return o;
            o << rhs.short_string[i];
        }
        return o;
    } 
private:
    char short_string[8] = {0};
};

static inline uint64_t convertStrToEightBytes(const std::string& input) {
    std::size_t len = input.length();
    if (len > 8) len = 8;
    char arr[8] = {0};
    std::memcpy(arr, input.data(), len);
    return *reinterpret_cast<uint64_t*>(arr);
}

static inline std::string convertEightBytesToString(const uint64_t bytes) {
    char arr[8];
    std::memcpy(arr, &bytes, 8);
    return std::string(arr);
}

static inline std::string getTimeStringFromTimestamp(int64_t timestamp) {
    int64_t one_billion = 1000000000;
    int64_t seconds = timestamp / one_billion;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;
    seconds = seconds % 60;
    minutes = minutes % 60;
    std::string minstr = std::to_string(minutes);
    if (minutes < 10)
        minstr.insert(0, 1, '0');
    std::string secstr = std::to_string(seconds);
    if (seconds < 10)
        secstr.insert(0, 1, '0');
    return std::to_string(hours) + ":" + minstr
        + ":" + secstr;
}
static inline uint16_t getTerminalWidth() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

static inline uint16_t getTerminalHeight() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row;
}   
}

#endif
