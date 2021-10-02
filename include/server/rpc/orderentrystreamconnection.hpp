#ifndef BIDIRECTIONAL_RPC_JOB_HPP
#define BIDIRECTIONAL_RPC_JOB_HPP

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
#include <thread>
#include <list>
#include <atomic>
#include <mutex>
#include <chrono>

#include "logger.hpp"
#include "orderentry.grpc.pb.h"
#include "util.hpp"
#include "order.hpp"
#include "ordertypes.hpp"
#include "orderentryjobhandlers.hpp"

using OERequestType = orderentry::OrderEntryRequest;
using OEResponseType = orderentry::OrderEntryResponse;
using ServiceType = orderentry::OrderEntryService::AsyncService;
using TagProcessor = std::function<void(bool)>;
using Rejection = orderentry::OrderEntryRejection::RejectionReason;
using Common = orderentry::OrderCommon;

class OrderEntryStreamConnection final {
public:
    OrderEntryStreamConnection(
        ServiceType* service, 
        grpc::ServerCompletionQueue* completion_q, 
        std::unordered_map<uint64_t, OrderEntryStreamConnection*>& client_streams,
        OEJobHandlers& jobhandlers
    );
    const uint64_t getUserID() const {return userid_;}
    void writeToClient(const OEResponseType* response);
    void sendRejection(const Rejection rejection, const uint64_t userid,
        const uint64_t orderid, const uint64_t ticker);
    void onStreamCancelled(bool); // notification tag callback for stream termination
    alignas(64) static std::atomic<uint64_t> orderid_generator_; // dont want to false share the orderid generator with current_async_ops
    static std::chrono::_V2::system_clock::time_point t0;
private:
    void sendResponseFromQueue(bool success);
    void initialiseOEConn(bool success);
    void verifyID(bool success);
    void readOrderEntryCallback(bool success);
    void processEntry();
    bool userIDUsageRejection(const Common& common);
    template<typename OrderType>
    void handleOrderType(const OrderType& order);
    void acknowledgeEntry(const orderentry::NewOrder& new_order);
    void acknowledgeEntry(const orderentry::ModifyOrder& modify_order);
    void acknowledgeEntry(const orderentry::CancelOrder& cancel_order);
    void processOrderEntry(const orderentry::NewOrder& new_order);
    void processOrderEntry(const orderentry::ModifyOrder& modify_order);
    void processOrderEntry(const orderentry::CancelOrder& cancel_order);
    void terminateConnection();
    void asyncOpStarted();
    void asyncOpFinished();
    std::string getUserAddress();

    ServiceType* service_;
    grpc::ServerCompletionQueue* completion_queue_;
    grpc::ServerContext server_context_;
    grpc::Alarm alarm_;
    OERequestType oe_request_;
    grpc::ServerAsyncReaderWriter<OEResponseType, OERequestType> grpc_responder_;
    TagProcessor initialise_oe_conn_callback_;
    TagProcessor read_orderentry_callback_;
    TagProcessor on_finish_;
    TagProcessor verify_userid_callback_;
    TagProcessor stream_cancellation_callback_;
    TagProcessor null_callback_;
    TagProcessor sendResponseFromQueue_cb_;
    std::function<void(tradeorder::Order&)> add_order_fn_;
    std::function<void(info::ModifyOrder&)> modify_order_fn_;
    std::function<void(info::CancelOrder&)> cancel_order_fn_;
    std::function<void()> create_new_conn_fn_;
    std::list<OEResponseType> response_queue_;
    std::list<OERequestType> request_queue_;
    std::mutex response_queue_mutex_;
    std::unordered_map<uint64_t, OrderEntryStreamConnection*>& client_streams_;
    uint64_t userid_;
    std::string user_address_;
    std::atomic<uint64_t> current_async_ops_;
    bool server_stream_done_;
    bool on_streamcancelled_called_;
    bool write_in_progress_ = false;

    static thread_local OEResponseType neworder_ack; 
    static thread_local OEResponseType modorder_ack; 
    static thread_local OEResponseType cancelorder_ack; 
    static thread_local OEResponseType rejection_ack;
};

template<typename OrderType>
inline void OrderEntryStreamConnection::handleOrderType(const OrderType& order) {
    if (userIDUsageRejection(order.order_common())) {
        return;
    }
    acknowledgeEntry(order);
    processOrderEntry(order);
}

#endif
