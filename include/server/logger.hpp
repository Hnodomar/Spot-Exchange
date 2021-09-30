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
#include <cassert>
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
        case LogType::Info: return output << "[INFO]";
        case LogType::Debug: return output << "[DEBUG]";
        case LogType::Warning: return output << "[WARNING]";
        case LogType::Error: return output << "[ERROR]";
        default: return output;
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
    static void format(std::ostream& o) {
        o << "\n";
    }
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
    std::aligned_storage<120, 8>::type log_entry_;
};

// FIFO Multi Producer Single Consumer queue for trade engine logging
template<typename DataType> 
class Queue {
public:
    Queue(const std::size_t size)
     : size_(size)
     , buffer_(static_cast<QueueIndex*>(std::malloc(sizeof(QueueIndex) * size)))
     , head_(0)
     , tail_(0) {
        const bool is_power_of_two = size && !(size & (size - 1));
        assert(is_power_of_two);
        if (!buffer_) throw std::bad_alloc();
    }
    ~Queue() {
        while (front()) pop();
        std::free(buffer_);
    }
    template<typename ...Args>
    void emplace(Args&& ...args) {
        const auto head = head_.fetch_add(1);
        auto& idx = buffer_[head & (size_ - 1)];
        while ((head & (size_ - 1)) - tail_.load(std::memory_order_acquire) >= size_);
        new (&(idx.log_entry)) DataType(std::forward<Args>(args)...);
        idx.is_constructed.store(true, std::memory_order_release);
    }
    DataType* front() {
        auto tail = tail_.load(std::memory_order_relaxed);
        if (!buffer_[tail].is_constructed.load(std::memory_order_acquire))
            return nullptr;
        return &(buffer_[tail].log_entry);
    }
    void pop() {
        auto tail = tail_.load(std::memory_order_relaxed);
        auto& idx = buffer_[tail];
        while (!idx.is_constructed.load(std::memory_order_acquire));
        idx.log_entry.~DataType();
        idx.is_constructed.store(false, std::memory_order_release);
        tail_.store(((tail + 1) & (size_ - 1)), std::memory_order_relaxed);
    }
private:
    struct QueueIndex {
        DataType log_entry;
        std::atomic<bool> is_constructed = false;
    };
    const std::size_t size_;
    QueueIndex* const buffer_;  
    alignas(64) std::atomic<std::size_t> head_; // force x86 cacheline alignment requirement to avoid false sharing
    alignas(64) std::atomic<std::size_t> tail_; // https://en.wikipedia.org/wiki/False_sharing
};
// Ideally: [loglevel] [datetime] [threadname:threadid] [msg] [module]
class Logger {
public:
    template<typename ...Args>
    static void Log(LogType type, Args&& ...args) {
        getQueue().emplace(type, std::forward<Args>(args)...);
    }
    static void setOutputFile(const std::string filename) {
        Logger& logger = getInstance();
        logger.use_std_out_ = false;
        logger.output_ = std::make_unique<std::ofstream>(filename);
    }
    Logger()
        : queue_size_(1024)
        , queue_(std::make_unique<Queue<LogEntry>>(queue_size_))
        , active_(true)
        , use_std_out_(true)
    {
        thread_ = std::thread([this]{writeFromQueue();});
    }
    ~Logger() {
        active_ = false;
        thread_.join();
    }
private:
    void writeFromQueue() {
        while (active_) {
            const auto log_entry = queue_->front();
            if (!log_entry) continue;
            if (output_) log_entry->formatLogEntry(*(output_.get()));
            else if (use_std_out_) log_entry->formatLogEntry(std::cout);
            queue_->pop();
        }
    }
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    static Queue<LogEntry>& getQueue() {
        return *(getInstance().queue_.get());
    }
    std::size_t queue_size_;
    std::unique_ptr<Queue<LogEntry>> queue_;
    std::unique_ptr<std::ostream> output_;
    std::thread thread_;
    bool active_;
    bool use_std_out_;
};
}

#endif
