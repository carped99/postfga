
#include "openfga_client.hpp"
#include "payload.h"
#include "request_variant.hpp"
#include "util/logger.hpp"

namespace postfga::client
{
    namespace
    {
        void fill_check_request(const Config &config, const CheckTuple& in, ::openfga::v1::CheckRequest& out)
        {
            const FgaCheckTupleRequest& payload = in.request();
            const FgaTuple& tuple = payload.tuple;
            if (payload.store_id[0] != '\0')
                out.set_store_id(payload.store_id);
            else
                out.set_store_id(config.store_id);

            out.set_consistency(::openfga::v1::ConsistencyPreference::HIGHER_CONSISTENCY);

            auto* tuple_key = out.mutable_tuple_key();

            tuple_key->set_object(std::string(tuple.object_type) + ":" + tuple.object_id);
            tuple_key->set_user(std::string(tuple.subject_type) + ":" + tuple.subject_id);
            tuple_key->set_relation(tuple.relation);
        }

        void fill_tuple_key(const FgaTuple& tuple, ::openfga::v1::CheckRequestTupleKey* tupleKey)
        {
            tupleKey->set_object(std::string(tuple.object_type) + ":" + tuple.object_id);
            tupleKey->set_user(std::string(tuple.subject_type) + ":" + tuple.subject_id);
            tupleKey->set_relation(tuple.relation);
        }

        struct CheckContext
        {
            grpc::ClientContext context;
            ::openfga::v1::CheckRequest request;
            ::openfga::v1::CheckResponse response;
        };

        struct BatchCheckContext
        {
            grpc::ClientContext context;
            ::openfga::v1::BatchCheckRequest request;
            ::openfga::v1::BatchCheckResponse response;
        };
    } // anonymous namespace

    void OpenFgaGrpcClient::handle_check_batch(std::vector<BatchCheckItem> items)
    {
        auto ctx = std::make_shared<BatchCheckContext>();

        ctx->request.set_store_id(config_.store_id);
        ctx->request.set_consistency(::openfga::v1::ConsistencyPreference::HIGHER_CONSISTENCY);
        for (const auto& item : items)
        {
            ::openfga::v1::BatchCheckItem *check = ctx->request.add_checks();
            check->set_correlation_id(std::to_string(item.params.request_id()));

            const FgaCheckTupleRequest& request = item.params.request();
            ::openfga::v1::CheckRequestTupleKey* tupleKey = check->mutable_tuple_key();
            fill_tuple_key(request.tuple, tupleKey);
        }
        
        // Set deadline
        ctx->context.set_deadline(std::chrono::system_clock::now() + config_.timeout);

        auto callback = [ctx, items = std::move(items)](::grpc::Status status)
        {
            if (status.ok())
            {
                const auto& result_map = ctx->response.result();
                for (auto& item : items)
                {
                    const auto& correlation_id = std::to_string( item.params.request_id() );
                    const auto& it = result_map.find(correlation_id);
                    FgaResponse& out = item.params.response();
                    if (it == result_map.end())
                    {
                        out.body.checkTuple.allow = false;
                        out.status = FGA_RESPONSE_CLIENT_ERROR;
                    }
                    else
                    {
                        const auto& res = it->second;
                        if (res.has_allowed())
                        {
                            out.status = FGA_RESPONSE_OK;
                            out.body.checkTuple.allow = res.allowed();
                        }
                        else if (res.has_error())
                        {
                            out.status = FGA_RESPONSE_SERVER_ERROR;
                            out.body.checkTuple.allow = false;
                            strlcpy(out.error_message, res.error().message().c_str(), sizeof(out.error_message));
                        }
                        else
                        {
                            out.status = FGA_RESPONSE_CLIENT_ERROR;
                            out.body.checkTuple.allow = false;
                            strlcpy(out.error_message, "Invalid response received", sizeof(out.error_message));
                        }
                    }
                    item.callback();
                }
            }
            else
            {
                for (auto& item : items)
                {
                    FgaResponse& out = item.params.response();
                    out.body.checkTuple.allow = false;
                    out.status = FGA_RESPONSE_CLIENT_ERROR;
                    strlcpy(out.error_message, status.error_message().c_str(), sizeof(out.error_message));
                    item.callback();
                }
            }
        };

        stub_->async()->BatchCheck(&ctx->context, &ctx->request, &ctx->response, std::move(callback));
    }


    void OpenFgaGrpcClient::handle_request(CheckTuple& req, ProcessCallback cb)
    {
        auto ctx = std::make_shared<CheckContext>();
        fill_check_request(config_, req, ctx->request);
        
        // Set deadline
        ctx->context.set_deadline(std::chrono::system_clock::now() + config_.timeout);

        auto callback = [ctx, req, cb = std::move(cb)](::grpc::Status status)
        {
            FgaResponse& res = req.response();
            if (status.ok())
            {
                res.status = FGA_RESPONSE_OK;
                res.body.checkTuple.allow = ctx->response.allowed();
            }
            else
            {
                res.status = FGA_RESPONSE_CLIENT_ERROR;
                res.body.checkTuple.allow = false;
                strlcpy(res.error_message, status.error_message().c_str(), sizeof(res.error_message));
            }
            cb();
        };

        stub_->async()->Check(&ctx->context, &ctx->request, &ctx->response, std::move(callback));
    }
} // namespace postfga::client
