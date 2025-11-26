
#include "openfga_handler.hpp"
#include "request_variant.hpp"

namespace postfga::client
{
    namespace
    {
        openfga::v1::CheckRequest make_check_request(const CheckTupleRequest &in)
        {
            openfga::v1::CheckRequest out;
            auto *tuple_key = out.mutable_tuple_key();
            // tuple_key->set_object(in.object);
            // tuple_key->set_user(in.user);
            // tuple_key->set_relation(in.relation);
            return out;
        }

        struct CheckContext
        {
            grpc::ClientContext ctx;
            openfga::v1::CheckRequest req;
            openfga::v1::CheckResponse res;

            // GrpcAsyncSlot *ga;          // shared memory slot 핸들
            // OpenFGAService::Stub *stub; // 필요하면
        };
    } // anonymous namespace

    FgaResponse OpenFgaHandler::handle_request(const CheckTupleRequest &req) const
    {
        FgaResponse out{};

        auto context = std::make_shared<CheckContext>();

        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(config_.timeout_ms);
        context->ctx.set_deadline(deadline);
        context->req = make_check_request(req);

        stub_->async()->Check(
            &context->ctx,
            &context->req,
            &context->res,
            [context = std::move(context)](grpc::Status status) mutable
            {
                if (status.ok())
                {
                    // out.allowed = gresp.allowed() ? 1 : 0;
                }
                else
                {
                    // out.status_code = static_cast<int>(status.error_code());
                    // std::strncpy(out.error_message, status.error_message().c_str(), sizeof(out.error_message) - 1);
                }
                // out.status_code = 0;
            });

        // auto greq = make_check_request(req);
        // openfga::v1::CheckResponse gresp;
        // grpc::Status status = stub_->Check(&ctx, greq, &gresp);
        // if (status.ok())
        // {
        //     // out.allowed = gresp.allowed() ? 1 : 0;
        // }
        // else
        // {
        //     // out.status_code = static_cast<int>(status.error_code());
        //     // std::strncpy(out.error_message, status.error_message().c_str(), sizeof(out.error_message) - 1);
        // }
        // // out.status_code = 0;
        // return out;
    }
} // namespace postfga::client