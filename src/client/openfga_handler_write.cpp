
#include "openfga_handler.hpp"
#include "request_variant.hpp"

namespace postfga::client
{
    namespace
    {
        openfga::v1::CheckRequest make_write_request(const WriteTupleRequest &in)
        {
            openfga::v1::CheckRequest out;
            auto *tuple_key = out.mutable_tuple_key();
            // tuple_key->set_object(in.object);
            // tuple_key->set_user(in.user);
            // tuple_key->set_relation(in.relation);
            return out;
        }
    } // anonymous namespace

    FgaResponse OpenFgaHandler::handle_request(const WriteTupleRequest &req) const
    {
        FgaResponse out{};

        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(config_.timeout_ms);
        grpc::ClientContext ctx;
        ctx.set_deadline(deadline);

        auto greq = make_write_request(req);
        openfga::v1::CheckResponse gresp;
        grpc::Status status = stub_->Check(&ctx, greq, &gresp);
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
        return out;
    }
} // namespace postfga::client