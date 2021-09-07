#ifndef UTIL_HPP
#define UTIL_HPP

#include <chrono>
#include <cstdint>

// misc utility functions
namespace util {
static int64_t getUnixTimestamp() {
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
}

#endif
