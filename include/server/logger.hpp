#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <iostream>
#include <fstream>
#include <memory>
#include <atomic>
#include <vector>
#include <thread>
#include <mutex>

namespace logging {
enum class LogType {
    Debug,
    Info,
    Warning,
    Error
};

class LogEntry {
public:
    template<typename ...Args> LogEntry(Args&& ...Args) {

    }
    ~LogEntry() {

    }
private:
    using Formatter = void(*)(std::ostream*, void*);
    Formatter formatter_;
};
template<typename DataType> 
class Queue {
public:
    Queue(const std::size_t size)
     : size_(size)
     , buffer_(static_cast<DataType*>(std::malloc(sizeof(DataType) * size)))
     , head_(0)
     , tail_(0) {
        if (!buffer_) throw std::bad_alloc();
    }
    ~Queue() {
        while (front()) pop();
        std::free(buffer_);
    }
    template<typename ...Args> 
    void emplace(Args&& ...args) {
        auto const head = head_.load(std::memory_order_relaxed);
        auto const next_head = (head + 1) % size_;
        while (next_head == tail_.load(std::memory_order_acquire));
        new (&buffer_[head]) DataType(std::forward<Args>(args)...);
        head_.store(next_head, std::memory_order_release);
    }
    DataType* front() {
        auto tail = tail_.load(std::memory_order_relaxed);
        if (head_.load(std::memory_order_acquire) == tail)
            return nullptr;
        return &buffer_[tail];
    }
    void pop() {
        auto tail = tail_.load(std::memory_order_relaxed);
        if (head_.load(std::memory_order_acquire) == tail)
            return;
        auto const next_tail = (tail + 1) % size_;
        buffer_[tail].~DataType();
        tail_.store(next_tail, std::memory_order_release);
    }
private:
    const std::size_t size_;
    const DataType* buffer_;
    std::atomic<std::size_t> head_, tail_;
};
// ideally: [loglevel] [datetime] [threadname:threadid] [msg] [module]
class Logger {
public:
    template<typename ...Args>
    static void LOG(const char* formatting, Args&& ...args) {
        getQueue().emplace(formatting, std::forward<Args>(args)...);
    }
private:
    Logger(const std::string& filename = "")
        : queue_size_(1024)
        , num_queues_(1)
        , use_std_out_(true)
        , active_(true)
    {
        if (filename != "")
            output_ = std::make_unique<std::ofstream>(filename);
        thread_ = std::thread([this]{writeFromQueue();});
    }
    ~Logger() {
        active_ = false;
        thread_.join();
    }
    void writeFromQueue() {
        while (active_ || num_queues_) {
            std::lock_guard<std::mutex> lock(getInstance().logger_mutex_);
            for (auto itr = queues_.begin(); itr != queues_.end();) {
                auto& queue = *itr;
                while (queue->front()) {
                    if (output_) {
                        queue->front()->Format(*(output_.get());
                    }
                    if (use_std_out_) {
                        queue->front()->Format(std::cout);
                    }
                    queue->pop();
                }
                if (queue.unique() && !queue->front()) {
                    itr = queues_.erase(itr);
                }
                else
                    ++itr;
            }
            num_queues_ = queues_.size();
        }
    }
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    static Queue<std::string>& getQueue() {
        static thread_local std::shared_ptr<Queue<std::string>> queue;
        if (queue == nullptr) {
            queue = std::make_shared<Queue<std::string>>();
            std::lock_guard<std::mutex> lock(getInstance().logger_mutex_);
            getInstance().queues_.push_back(queue);
        }
        return *(queue.get());
    }
    std::mutex logger_mutex_;
    std::size_t queue_size_;
    std::size_t num_queues_;
    std::vector<std::shared_ptr<Queue<std::string>>> queues_;
    std::unique_ptr<std::ostream> output_;
    std::thread thread_;
    bool active_;
    bool use_std_out_;

};
}

#endif
