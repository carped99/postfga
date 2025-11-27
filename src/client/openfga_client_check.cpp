
#include "request.h"
#include "openfga_client.hpp"
#include "request_variant.hpp"
#include "util/logger.hpp"

namespace postfga::client
{
    namespace
    {
        void fill_check_request(const CheckTupleRequest &in, openfga::v1::CheckRequest &out)
        {
            const FgaCheckTupleRequest &payload = in.payload();
            const FgaTuple &tuple = payload.tuple;
            out.set_store_id(payload.store_id);
            // out.set_consistency_mode(openfga::v1::CheckRequest::CONSISTENCY_MODE_STRONG);

            auto *tuple_key = out.mutable_tuple_key();

            tuple_key->set_object(tuple.object_id);

            std::string user = std::string(tuple.subject_type) + ":" + tuple.subject_id;
            tuple_key->set_user(user);
            tuple_key->set_relation(tuple.relation);
        }

        struct CheckContext
        {
            grpc::ClientContext ctx;
            openfga::v1::CheckRequest req;
            openfga::v1::CheckResponse res;
        };
    } // anonymous namespace

    void OpenFgaGrpcClient::handle_request(const CheckTupleRequest &req, FgaResponseHandler handler, void *ctx)
    {

        auto context = std::make_shared<CheckContext>();

        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(config_.timeout_ms);
        context->ctx.set_deadline(deadline);
        fill_check_request(req, context->req);

        postfga::util::info("OpenFgaGrpcClient: Check request received" + std::to_string(config_.timeout_ms));

        stub_->async()->Check(
            &context->ctx,
            &context->req,
            &context->res,
            [context = std::move(context), handler, ctx](grpc::Status status) mutable
            {
                fprintf(stderr, "OpenFgaGrpcClient: Check response received with status: %s\n", status.error_message().c_str());
                FgaResponse out{};
                if (status.ok())
                {
                    // out.allowed = gresp.allowed() ? 1 : 0;
                }
                else
                {
                    // out.status_code = static_cast<int>(status.error_code());
                    // std::strncpy(out.error_message, status.error_message().c_str(), sizeof(out.error_message) - 1);
                }

                handler(out, ctx);
                // out.status_code = 0;
            });
    }

    void OpenFgaGrpcClient::handle_request(const WriteTupleRequest &req, FgaResponseHandler handler, void *ctx)
    {
    }
    void OpenFgaGrpcClient::handle_request(const DeleteTupleRequest &req, FgaResponseHandler handler, void *ctx)
    {
    }
    void OpenFgaGrpcClient::handle_request(const GetStoreRequest &req, FgaResponseHandler handler, void *ctx)
    {
    }
    void OpenFgaGrpcClient::handle_request(const CreateStoreRequest &req, FgaResponseHandler handler, void *ctx)
    {
    }
} // namespace postfga::client