#ifndef RPC_JOB_HPP
#define RPC_JOB_HPP

#include <cstdint>

class RPCJob {
public:
    enum class AsyncOperationType {
        INVALID,
        QUEUED_REQUEST,
        READ,
        WRITE,
        FINISH
    }; 
    RPCJob() :
        curr_async_ops_(0),
        user_id_(id_generator_++),
        read_in_progress_(false),
        write_in_progress_(false),
        on_done_called_(false)
    {}
    virtual ~RPCJob() {}
    void asyncOperationStarted(AsyncOperationType op) {
        ++curr_async_ops_;
        switch(op) {
            case AsyncOperationType::READ:
                read_in_progress_ = true;
                break;
            case AsyncOperationType::WRITE:
                write_in_progress_ = true;
                break;
            default:
                break;
        }
    }
    bool asyncOperationFinished(AsyncOperationType op) {
        --curr_async_ops_;
        switch(op) {
            case AsyncOperationType::READ:
                read_in_progress_ = false;
                break;
            case AsyncOperationType::WRITE:
                write_in_progress_ = false;
                break;
            default:
                break;
        }
        if (curr_async_ops_ == 0 && on_done_called_) {
            done();
            return false;
        }
        return true;
    }
    bool isAsyncReadInProgress() const {
        return read_in_progress_;
    }
    bool isAsyncWriteInProgress() const {
        return write_in_progress_;
    }
    void onDone(bool ) {
        on_done_called_ = true;
        if (curr_async_ops_ == 0)
            done();
    }
    virtual void done() = 0;
    uint64_t getUserID() const {return user_id_;}
private:
    inline static uint64_t id_generator_ = 0;
    int32_t curr_async_ops_;
    const uint64_t user_id_;
    bool read_in_progress_;
    bool write_in_progress_;
    bool on_done_called_;
};

#endif
