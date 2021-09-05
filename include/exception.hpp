#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include <stdexcept>

struct EngineException : public std::runtime_error {
    EngineException(const std::string& msg):
        runtime_error(msg)
    {}
};

#endif
