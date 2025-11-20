/*
 * grpc_client.cpp
 *
 * OpenFGA gRPC async client implementation using standalone ASIO
 */

#include "grpc_client.h"

#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

// Standalone ASIO
#include <asio.hpp>

// gRPC and OpenFGA proto
#include <grpcpp/grpcpp.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <openfga/v1/openfga.grpc.pb.h>
#include <openfga/v1/openfga.pb.h>

using namespace openfga::v1;

/* ========================================================================
 * Internal structures
 * ======================================================================== */

struct AsyncCheckCall {
    grpc::ClientContext context;
    CheckRequest request_params;
    CheckCallback callback;
    void *user_data;

    CheckRequest grpc_request;
    CheckResponse grpc_response;
    grpc::Status status;

    std::unique_ptr<grpc::ClientAsyncResponseReader<CheckResponse>> rpc;
};

struct GrpcClient {
    // gRPC channel and stub
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<OpenFGAService::Stub> stub;

    // Completion queue for async operations
    std::unique_ptr<grpc::CompletionQueue> cq;

    // ASIO io_context for event loop
    asio::io_context io_context;
    std::unique_ptr<std::thread> io_thread;
    std::atomic<bool> running{false};

    // Pending async calls
    std::mutex mutex;
    std::queue<std::unique_ptr<AsyncCheckCall>> pending_calls;

    // Statistics
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> total_errors{0};
};

/* ========================================================================
 * Helper functions
 * ======================================================================== */

static void fill_grpc_check_request(const CheckRequest *req,
                                     openfga::v1::CheckRequest *grpc_req)
{
    grpc_req->set_store_id(req->store_id);

    if (req->authorization_model_id && req->authorization_model_id[0] != '\0')
        grpc_req->set_authorization_model_id(req->authorization_model_id);

    // Set tuple key
    auto *tuple_key = grpc_req->mutable_tuple_key();

    // Object: "type:id"
    std::string object = std::string(req->object_type) + ":" + std::string(req->object_id);
    tuple_key->set_object(object);

    // Relation
    tuple_key->set_relation(req->relation);

    // User/Subject: "type:id"
    std::string user = std::string(req->subject_type) + ":" + std::string(req->subject_id);
    tuple_key->set_user(user);
}

static void fill_check_response(const openfga::v1::CheckResponse *grpc_resp,
                                const grpc::Status &status,
                                CheckResponse *resp)
{
    if (status.ok()) {
        resp->allowed = grpc_resp->allowed();
        resp->error_code = 0;
        resp->error_message[0] = '\0';
    } else {
        resp->allowed = false;
        resp->error_code = static_cast<uint32_t>(status.error_code());

        std::string error_msg = status.error_message();
        size_t copy_len = std::min(error_msg.length(), sizeof(resp->error_message) - 1);
        memcpy(resp->error_message, error_msg.c_str(), copy_len);
        resp->error_message[copy_len] = '\0';
    }
}

/* ========================================================================
 * IO thread for processing gRPC completion queue
 * ======================================================================== */

static void grpc_completion_queue_thread(GrpcClient *client)
{
    void *tag;
    bool ok;

    while (client->running) {
        // Wait for next completion queue event with timeout
        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(100);
        auto result = client->cq->AsyncNext(&tag, &ok, deadline);

        if (result == grpc::CompletionQueue::GOT_EVENT) {
            if (ok) {
                // Call completed successfully
                auto *call = static_cast<AsyncCheckCall *>(tag);

                // Fill response
                CheckResponse response;
                fill_check_response(&call->grpc_response, call->status, &response);

                // Invoke callback
                if (call->callback) {
                    call->callback(&response, call->user_data);
                }

                // Cleanup
                delete call;
            }
        } else if (result == grpc::CompletionQueue::TIMEOUT) {
            // Timeout, continue loop
            continue;
        } else {
            // SHUTDOWN
            break;
        }
    }
}

/* ========================================================================
 * Public API implementation
 * ======================================================================== */

extern "C" {

GrpcClient *grpc_client_init(const char *endpoint)
{
    if (!endpoint || endpoint[0] == '\0')
        return nullptr;

    try {
        auto client = std::make_unique<GrpcClient>();

        // Create gRPC channel
        grpc::ChannelArguments args;
        args.SetMaxReceiveMessageSize(4 * 1024 * 1024);  // 4MB
        args.SetMaxSendMessageSize(4 * 1024 * 1024);

        client->channel = grpc::CreateCustomChannel(
            endpoint,
            grpc::InsecureChannelCredentials(),
            args
        );

        if (!client->channel) {
            return nullptr;
        }

        // Create stub
        client->stub = OpenFGAService::NewStub(client->channel);

        // Create completion queue
        client->cq = std::make_unique<grpc::CompletionQueue>();

        // Start IO thread for completion queue
        client->running = true;
        client->io_thread = std::make_unique<std::thread>(
            grpc_completion_queue_thread,
            client.get()
        );

        return client.release();

    } catch (const std::exception &e) {
        return nullptr;
    }
}

void grpc_client_shutdown(GrpcClient *client)
{
    if (!client)
        return;

    // Stop IO thread
    client->running = false;

    // Shutdown completion queue
    client->cq->Shutdown();

    // Wait for IO thread
    if (client->io_thread && client->io_thread->joinable()) {
        client->io_thread->join();
    }

    // Cleanup
    delete client;
}

bool grpc_client_check_sync(GrpcClient *client,
                            const CheckRequest *request,
                            CheckResponse *response)
{
    if (!client || !request || !response)
        return false;

    try {
        grpc::ClientContext context;

        // Set timeout
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

        // Prepare gRPC request
        openfga::v1::CheckRequest grpc_req;
        fill_grpc_check_request(request, &grpc_req);

        // Make synchronous call
        openfga::v1::CheckResponse grpc_resp;
        grpc::Status status = client->stub->Check(&context, grpc_req, &grpc_resp);

        // Fill response
        fill_check_response(&grpc_resp, status, response);

        client->total_requests++;
        if (!status.ok())
            client->total_errors++;

        return status.ok();

    } catch (const std::exception &e) {
        response->allowed = false;
        response->error_code = 1000;  // Internal error
        snprintf(response->error_message, sizeof(response->error_message),
                "Exception: %s", e.what());
        client->total_errors++;
        return false;
    }
}

bool grpc_client_check_async(GrpcClient *client,
                             const CheckRequest *request,
                             CheckCallback callback,
                             void *user_data)
{
    if (!client || !request || !callback)
        return false;

    try {
        auto call = std::make_unique<AsyncCheckCall>();

        // Copy request parameters
        call->request_params = *request;
        call->callback = callback;
        call->user_data = user_data;

        // Set timeout
        call->context.set_deadline(
            std::chrono::system_clock::now() + std::chrono::seconds(5)
        );

        // Prepare gRPC request
        fill_grpc_check_request(request, &call->grpc_request);

        // Start async call
        call->rpc = client->stub->AsyncCheck(
            &call->context,
            call->grpc_request,
            client->cq.get()
        );

        // Set tag to identify this call when it completes
        call->rpc->Finish(&call->grpc_response, &call->status, (void *)call.get());

        client->total_requests++;

        // Release ownership (will be deleted in completion queue thread)
        call.release();

        return true;

    } catch (const std::exception &e) {
        client->total_errors++;
        return false;
    }
}

int grpc_client_poll(GrpcClient *client, int timeout_ms)
{
    if (!client)
        return 0;

    // In this implementation, polling is handled by the IO thread
    // This function can be used for additional processing if needed

    // For now, just return 0 (IO thread handles everything)
    return 0;
}

bool grpc_client_is_healthy(GrpcClient *client)
{
    if (!client || !client->channel)
        return false;

    try {
        auto state = client->channel->GetState(true);
        return (state == GRPC_CHANNEL_READY || state == GRPC_CHANNEL_IDLE);
    } catch (...) {
        return false;
    }
}

} // extern "C"
