
#include "openfga_client.hpp"
#include "request.h"
#include "request_variant.hpp"
#include "util/logger.hpp"

namespace postfga::client
{
    namespace
    {
        void fill_check_request(const CheckTupleRequest& in, openfga::v1::CheckRequest& out)
        {
            const FgaCheckTupleRequest& payload = in.payload();
            const FgaTuple& tuple = payload.tuple;
            out.set_store_id(payload.store_id);
            // out.set_consistency_mode(openfga::v1::CheckRequest::CONSISTENCY_MODE_STRONG);

            auto* tuple_key = out.mutable_tuple_key();

            tuple_key->set_object(std::string(tuple.object_type) + ":" + tuple.object_id);
            tuple_key->set_user(std::string(tuple.subject_type) + ":" + tuple.subject_id);
            tuple_key->set_relation(tuple.relation);
        }

        struct CheckContext
        {
            grpc::ClientContext ctx;
            openfga::v1::CheckRequest req;
            openfga::v1::CheckResponse res;
        };
    } // anonymous namespace

    void OpenFgaGrpcClient::handle_request(const CheckTupleRequest& req, FgaResponseHandler handler, void* ctx)
    {
        auto client = shared_from_this();
        // auto context = std::make_shared<CheckContext>();
        auto context = CheckContext{};

        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(config_.timeout_ms);
        context.ctx.set_deadline(deadline);
        fill_check_request(req, context.req);

        std::mutex mu;
        std::condition_variable cv;
        bool done = false;

        // postfga::util::info(std::format("OpenFgaGrpcClient: Pre stub={}", context->req.DebugString().c_str()));

        stub_->async()->Check(&context.ctx,
                              &context.req,
                              &context.res,
                              [client, &context, &done, &cv, &mu](grpc::Status status) mutable
                              {
                                  fprintf(stderr,
                                          "OpenFgaGrpcClient: Check response received with status: %s\n",
                                          status.error_message().c_str());

                                  std::lock_guard<std::mutex> lock(mu);
                                  done = true;
                                  cv.notify_one();
                              });

        postfga::util::info(std::format("OpenFgaGrpcClient: Post1 stub={}", context.req.DebugString().c_str()));

        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [&done] { return done; });
        postfga::util::info(std::format("OpenFgaGrpcClient: Post2 stub={}", context.req.DebugString().c_str()));
    }

    void OpenFgaGrpcClient::handle_request(const WriteTupleRequest& req, FgaResponseHandler handler, void* ctx) {}
    void OpenFgaGrpcClient::handle_request(const DeleteTupleRequest& req, FgaResponseHandler handler, void* ctx) {}
    void OpenFgaGrpcClient::handle_request(const GetStoreRequest& req, FgaResponseHandler handler, void* ctx) {}
    void OpenFgaGrpcClient::handle_request(const CreateStoreRequest& req, FgaResponseHandler handler, void* ctx) {}
} // namespace postfga::client
