
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

    void OpenFgaGrpcClient::handle_request(const CheckTupleRequest& req,
                                           FgaResponse& res,
                                           FgaResponseHandler handler,
                                           void* ctx)
    {
        using Callback = std::function<void(::grpc::Status)>;

        auto context = std::make_shared<CheckContext>();
        fill_check_request(req, context->req);

        // Set deadline
        context->ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(config_.timeout_ms));

        auto* ptr = context.get();

        Callback callback = [context, res, handler, ctx](::grpc::Status status) mutable
        {
            fprintf(
                stderr, "OpenFgaGrpcClient: Check response received with status: %s\n", status.error_message().c_str());
            if (status.ok())
            {
                res.status = FGA_RESPONSE_OK;
            }
            else
            {
                res.status = FGA_RESPONSE_CLIENT_ERROR;
            }

            handler(res, ctx);

            // 여기서 context를 써도 OK. 콜백 끝날 때까지 살아 있음.
        };

        stub_->async()->Check(&ptr->ctx, &ptr->req, &ptr->res, callback);

        // std::unique_lock<std::mutex> lock(mu);
        // cv.wait(lock, [&done] { return done; });
        postfga::util::info("OpenFgaGrpcClient: Post2");
    }

    void OpenFgaGrpcClient::handle_request(const WriteTupleRequest& req,
                                           FgaResponse& res,
                                           FgaResponseHandler handler,
                                           void* ctx)
    {
    }
    void OpenFgaGrpcClient::handle_request(const DeleteTupleRequest& req,
                                           FgaResponse& res,
                                           FgaResponseHandler handler,
                                           void* ctx)
    {
    }
    void OpenFgaGrpcClient::handle_request(const GetStoreRequest& req,
                                           FgaResponse& res,
                                           FgaResponseHandler handler,
                                           void* ctx)
    {
    }
    void OpenFgaGrpcClient::handle_request(const CreateStoreRequest& req,
                                           FgaResponse& res,
                                           FgaResponseHandler handler,
                                           void* ctx)
    {
    }
} // namespace postfga::client
