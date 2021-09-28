#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <iostream>
#include <fstream>
#include <memory>
#include <atomic>
#include <vector>
#include <tuple>
#include <thread>
#include <string_view>
#include <type_traits>
#include <mutex>

namespace logging {
enum class LogType {
    Debug,
    Info,
    Warning,
    Error
};

inline std::ostream& operator<<(std::ostream& output, LogType type) {
    switch(type) {
        case LogType::Info: return output << "INFO";
        case LogType::Debug: return output << "DEBUG";
        case LogType::Warning: return output << "WARNING";
        case LogType::Error: return output << "ERROR";
    }
}

class LogEntry {
public:
    template<typename ...Args> LogEntry(Args&& ...args) {
        formatter_ = packLogEntry(&log_entry_, std::forward<Args>(args)...);
    }
    ~LogEntry() {formatter_(nullptr, &log_entry_);}
    void formatLogEntry(std::ostream& output) {formatter_(&output, &log_entry_);}
private:
    static void format(std::ostream& output) {output << "\n";}
    template <typename T, typename... Args>
    static void format(std::ostream &o, T &&t, Args &&... args) {
        o << t << " ";
        format(o, std::forward<Args>(args)...);
    }
    template<typename Tuple, std::size_t ...index>
    static void callFormatWithTupleValues(std::ostream& output, Tuple&& args, std::index_sequence<index...>) {
        format(output, std::get<index>(std::forward<Tuple>(args))...);
    }
    template<typename ...Args>
    static void makeTupleAndSequenceIt(std::ostream* output, void* log_entry) {
        using Tuple = std::tuple<Args...>;
        Tuple& args = *reinterpret_cast<Tuple*>(log_entry);
        if (output)
            callFormatWithTupleValues(*output, args, std::index_sequence_for<Args...>{});
        else 
            args.~Tuple();
    }
    template<typename T, typename ...Args>
    static decltype(auto) packLogEntry(T* entry_storage, Args&& ...args) {
        using Tuple = std::tuple<typename std::decay<Args>::type...>;
        static_assert(alignof(Tuple) <= alignof(T));
        static_assert(sizeof(Tuple) <= sizeof(T));
        new(entry_storage) Tuple(std::forward<Args>(args)...);
        return &makeTupleAndSequenceIt<typename std::decay<Args>::type...>;
    }
    using Formatter = void(*)(std::ostream*, void*);
    Formatter formatter_;
    std::aligned_storage<56, 8>::type log_entry_;
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
    DataType* const buffer_;
    std::atomic<std::size_t> head_, tail_;
};
// Ideally: [loglevel] [datetime] [threadname:threadid] [msg] [module]
class Logger {
public:
    template<typename ...Args>
    static void Log(LogType type, Args&& ...args) {
        getQueue().emplace(type, std::forward<Args>(args)...);
    }
    Logger(const std::string_view filename = "")
        : queue_size_(1024)
        , num_queues_(1)
        , active_(true)
        , use_std_out_(true)
    {
        if (filename != "")
            output_ = std::make_unique<std::ofstream>(std::string(filename));
        thread_ = std::thread([this]{writeFromQueue();});
    }

    ~Logger() {
        active_ = false;
        thread_.join();
    }
private:
    void writeFromQueue() {
        while (active_ || num_queues_) {
            std::lock_guard<std::mutex> lock(getInstance().logger_mutex_);
            for (auto itr = queues_.begin(); itr != queues_.end();) {
                auto& queue = *itr;
                while (queue->front()) {
                    if (output_) {
                        queue->front()->formatLogEntry(*(output_.get()));
                    }
                    if (use_std_out_) {
                        queue->front()->formatLogEntry(std::cout);
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
    static Queue<LogEntry>& getQueue() {
        static thread_local std::shared_ptr<Queue<LogEntry>> queue;
        if (queue == nullptr) {
            queue = std::make_shared<Queue<LogEntry>>(getInstance().queue_size_);
            std::lock_guard<std::mutex> lock(getInstance().logger_mutex_);
            getInstance().queues_.push_back(queue);
        }
        return *(queue.get());
    }
    std::mutex logger_mutex_;
    std::size_t queue_size_;
    std::size_t num_queues_;
    std::vector<std::shared_ptr<Queue<LogEntry>>> queues_;
    std::unique_ptr<std::ostream> output_;
    std::thread thread_;
    bool active_;
    bool use_std_out_;
};
}

#endif
