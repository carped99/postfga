
#include "openfga_client.hpp"
#include "payload.h"
#include "request_variant.hpp"
#include "util/logger.hpp"

namespace postfga::client
{
    namespace
    {
        struct CreateStoreContext
        {
            grpc::ClientContext context;
            ::openfga::v1::CreateStoreRequest request;
            ::openfga::v1::CreateStoreResponse response;
        };
        
        struct DeleteStoreContext
        {
            grpc::ClientContext context;
            ::openfga::v1::DeleteStoreRequest request;
            ::openfga::v1::DeleteStoreResponse response;
        };
    } // anonymous namespace

    void OpenFgaGrpcClient::handle_request(CreateStore& req, ProcessCallback cb)
    {
        auto ctx = std::make_shared<CreateStoreContext>();
        ctx->request.set_name(req.request().name);
        
        // Set deadline
        ctx->context.set_deadline(std::chrono::system_clock::now() + config_.timeout);

        auto callback = [ctx, req, cb = std::move(cb)](::grpc::Status status)
        {
            FgaResponse& res = req.response();
            FgaCreateStoreResponse& body = res.body.createStore;
            if (status.ok())
            {
                res.status = FGA_RESPONSE_OK;
                strlcpy(body.id, ctx->response.id().c_str(), sizeof(body.id));
                strlcpy(body.name, ctx->response.name().c_str(), sizeof(body.name));
            }
            else
            {
                res.status = FGA_RESPONSE_CLIENT_ERROR;
                strlcpy(res.error_message, status.error_message().c_str(), sizeof(res.error_message));
            }
            cb();
        };

        stub_->async()->CreateStore(&ctx->context, &ctx->request, &ctx->response, std::move(callback));
    }

    void OpenFgaGrpcClient::handle_request(DeleteStore& req, ProcessCallback cb)
    {
        auto ctx = std::make_shared<DeleteStoreContext>();
        ctx->request.set_store_id(req.request().store_id);
        
        // Set deadline
        ctx->context.set_deadline(std::chrono::system_clock::now() + config_.timeout);

        auto callback = [ctx, req, cb = std::move(cb)](::grpc::Status status)
        {
            FgaResponse& res = req.response();
            if (status.ok())
            {
                res.status = FGA_RESPONSE_OK;
            }
            else
            {
                res.status = FGA_RESPONSE_CLIENT_ERROR;
                strlcpy(res.error_message, status.error_message().c_str(), sizeof(res.error_message));
            }
            cb();
        };

        stub_->async()->DeleteStore(&ctx->context, &ctx->request, &ctx->response, std::move(callback));
    }
} // namespace postfga::client
