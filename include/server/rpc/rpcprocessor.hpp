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
        std::mutex& serv_tags_mutex
        )
        : serv_tags_list_(serv_taglist_ref)
        , serv_tags_mutex_(serv_tags_mutex)
    {}
    void operator()() {
        for(;;) {
            serv_tags_mutex_.lock();
            callbacks_ = std::move(serv_tags_list_);
            serv_tags_mutex_.unlock();
            while (!callbacks_.empty()) {
                CallbackTag cb_tag = callbacks_.front();
                callbacks_.pop_front();
                (*(cb_tag.callback_fn))(cb_tag.ok);
            }
        }
    }
    bool is_waiting_;
private:
    std::mutex& serv_tags_mutex_;
    std::list<CallbackTag> callbacks_;
    std::list<CallbackTag>& serv_tags_list_;
};

}
#endif
