#ifndef RPC_PROCESSOR_HPP
#define RPC_PROCESSOR_HPP

#include <condition_variable>
#include <mutex>
#include <functional>
#include <list>

namespace RPC {
struct CallbackTag {
    using Callback = std::function<void(bool)>;
    Callback* callback_fn;
    bool ok;
};
struct RPCProcessor {
    RPCProcessor(
        std::list<CallbackTag>& serv_taglist_ref,
        std::mutex& serv_tags_mutex,
        std::condition_variable& condv)
        : serv_tags_list_(serv_taglist_ref)
        , serv_tags_mutex_(serv_tags_mutex)
        , condv_(condv)
    {}
    void operator()() {
        std::unique_lock<std::mutex> lk(serv_tags_mutex_);
        for(;;) {
            condv_.wait(
                lk, 
                [this](){return !serv_tags_list_.empty();}
            );
            callbacks_ = std::move(serv_tags_list_);
            serv_tags_mutex_.unlock();
            while (!callbacks_.empty()) {
                CallbackTag cb_tag = callbacks_.front();
                callbacks_.pop_front();
                (*(cb_tag.callback_fn))(cb_tag.ok);
            }
        }
    }
private:
    std::condition_variable& condv_;
    std::mutex& serv_tags_mutex_;
    std::list<CallbackTag> callbacks_;
    std::list<CallbackTag>& serv_tags_list_;
};

}
#endif
