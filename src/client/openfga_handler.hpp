#pragma once

#include <memory>
#include <grpcpp/grpcpp.h>

#include "openfga/v1/openfga_service.grpc.pb.h"
#include "config/config.hpp"
#include "request_variant.hpp"

namespace postfga::client {

class OpenFgaHandler {
public:
    explicit OpenFgaHandler(const postfga::Config& config, std::shared_ptr<grpc::Channel> channel);

    FgaResponse handle(const FgaRequest& req) const;

private:
    FgaResponse handle_request(const CheckTupleRequest& req) const;
    FgaResponse handle_request(const WriteTupleRequest& req) const;
    FgaResponse handle_request(const DeleteTupleRequest& req) const;
    FgaResponse handle_request(const GetStoreRequest& req) const;
    FgaResponse handle_request(const CreateStoreRequest& req) const;

private:
    Config config_;
    std::shared_ptr<grpc::Channel>                        channel_;
    std::unique_ptr<openfga::v1::OpenFGAService::Stub>    stub_;
};

} // namespace postfga::client