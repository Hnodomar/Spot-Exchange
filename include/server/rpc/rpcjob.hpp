#ifndef RPC_JOB_HPP
#define RPC_JOB_HPP

#include <cstdint>

class RPCJob {
public:
    enum class AsyncOpType {
        INVALID,
        QUEUED_REQUEST,
        READ,
        WRITE,
        FINISH
    }; 
    RPCJob() 
    {}
    virtual ~RPCJob() {}
private:

};


#endif
