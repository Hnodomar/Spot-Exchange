#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <iostream>
#include <fstream>
#include <memory>

namespace logging {
enum class LogType {
    Debug,
    Info,
    Warning,
    Error
};
// ideally: [loglevel] [datetime] [threadid] [threadname] [msg] [module]
class Logger {
public:
    Logger(const std::string& outputfile): outputstream_(nullptr) {
        if (!outputfile.empty()) {
            file_.open(outputfile, std::ios_base::app);
            outputstream_ = &file_;
        }
        else
            outputstream_ = &(std::cout);
    }
    void write(const std::string& output) {
        *outputstream_ << output << std::endl;
    }  
private:
    std::ostream* outputstream_;
    std::fstream file_;
};
}

#endif
