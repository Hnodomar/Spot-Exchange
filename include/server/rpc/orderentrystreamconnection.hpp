#ifndef BIDIRECTIONAL_RPC_JOB_HPP
#define BIDIRECTIONAL_RPC_JOB_HPP

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
#include <thread>
#include <list>
#include <atomic>
#include <mutex>

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

// to avoid constant reallocation of these message types, we re-use them
// and keep their size constant. zero-initialised: http://en.cppreference.com/w/cpp/language/initialization
extern thread_local OEResponseType neworder_ack; 
extern thread_local OEResponseType modifyorder_ack;
extern thread_local OEResponseType cancelorder_ack;
extern thread_local OEResponseType rejection_ack;

class OrderEntryStreamConnection {
public:
    OrderEntryStreamConnection(
        ServiceType* service, 
        grpc::ServerCompletionQueue* completion_q, 
        std::unordered_map<uint64_t, OrderEntryStreamConnection*>& client_streams,
        OEJobHandlers& jobhandlers
    );
    const uint64_t getUserID() const {return userid_;}
    void queueWrite(const OEResponseType* response);
    void sendRejection(const Rejection rejection, const uint64_t userid,
        const uint64_t orderid, const uint64_t ticker);
private:
    void writeFromQueue(bool success);
    void initialiseOEConn(bool success);
    void verifyID(bool success);
    void readOrderEntryCallback(bool success);
    void acknowledgeEntry();
    bool userIDUsageRejection(const Common& common);
    void sendNewOrderAcknowledgement(const orderentry::NewOrder& new_order);
    void sendModifyOrderAcknowledgement(const orderentry::ModifyOrder& modify_order);
    void sendCancelOrderAcknowledgement(const orderentry::CancelOrder& cancel_order);
    void processNewOrderEntry(bool success);
    void processModifyOrderEntry(bool success);
    void processCancelOrderEntry(bool success);
    void terminateConnection();
    void asyncOpStarted();
    void asyncOpFinished();
    void onStreamCancelled(bool); // notification tag callback for stream termination

    ServiceType* service_;
    grpc::ServerCompletionQueue* completion_queue_;
    grpc::ServerContext server_context_;
    grpc::Alarm alarm_;
    OERequestType oe_request_;
    grpc::ServerAsyncReaderWriter<OEResponseType, OERequestType> grpc_responder_;
    TagProcessor initialise_oe_conn_callback_;
    TagProcessor read_orderentry_callback_;
    TagProcessor process_neworder_entry_callback_;
    TagProcessor process_modifyorder_entry_callback_;
    TagProcessor process_cancelorder_entry_callback_;
    TagProcessor write_from_queue_callback_;
    TagProcessor on_finish_;
    TagProcessor verify_userid_callback_;
    TagProcessor stream_cancellation_callback_;
    TagProcessor null_callback_;
    std::function<void(tradeorder::Order&)> add_order_fn_;
    std::function<void(info::ModifyOrder&)> modify_order_fn_;
    std::function<void(info::CancelOrder&)> cancel_order_fn_;
    std::function<void()> create_new_conn_fn_;
    std::list<OEResponseType> response_queue_;
    std::list<OERequestType> request_queue_;
    std::mutex response_queue_mutex_;
    std::mutex request_queue_mutex_;
    std::unordered_map<uint64_t, OrderEntryStreamConnection*>& client_streams_;
    uint64_t userid_;
    static std::atomic<uint64_t> orderid_generator_;
    std::atomic<uint64_t> current_async_ops_;
    bool server_stream_done_;
    bool client_stream_done_;
    bool on_streamcancelled_called_;
    bool write_in_progress_;
};

#endif
