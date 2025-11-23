
#include "openfga_handler.hpp"
#include "request_variant.hpp"

namespace postfga::client {

OpenFgaHandler::OpenFgaHandler(const Config& config, 
                                std::shared_ptr<grpc::Channel> channel)
    : config_(config)
    , channel_(std::move(channel))
    , stub_(openfga::v1::OpenFGAService::NewStub(channel_))
{}    

FgaResponse OpenFgaHandler::handle (const FgaRequest& req) const
{
    // C POD -> C++ std::variant
    auto variant = make_request_variant(req);

    // dispatch
    return std::visit([this](const auto& v) {
        return this->handle_request(v);
    }, variant);
}

FgaResponse OpenFgaHandler::handle_request(const WriteTupleRequest& req) const
{
    FgaResponse out{};
    return out;
}

FgaResponse OpenFgaHandler::handle_request(const DeleteTupleRequest& req) const
{
    FgaResponse out{};
    return out;
}

FgaResponse OpenFgaHandler::handle_request(const GetStoreRequest& req) const
{
    FgaResponse out{};
    return out;
}

FgaResponse OpenFgaHandler::handle_request(const CreateStoreRequest& req) const
{
    FgaResponse out{};
    return out;
}

} // namespace postfga::client