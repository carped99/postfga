
#include "openfga_client.hpp"
#include "request.h"
#include "request_variant.hpp"
#include "util/logger.hpp"

namespace postfga::client
{
    namespace
    {
        void fill_check_request(const Config &config, const CheckTuple& in, ::openfga::v1::CheckRequest& out)
        {
            const FgaCheckTupleRequest& payload = in.payload();
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
            grpc::ClientContext ctx;
            ::openfga::v1::CheckRequest req;
            ::openfga::v1::CheckResponse res;
        };

        struct BatchCheckContext
        {
            grpc::ClientContext ctx;
            ::openfga::v1::BatchCheckRequest req;
            ::openfga::v1::BatchCheckResponse res;
        };
    } // anonymous namespace

    void OpenFgaGrpcClient::handle_check_batch(std::vector<BatchCheckItem> items)
    {
        auto context = std::make_shared<BatchCheckContext>();

        for (const auto& item : items)
        {
            ::openfga::v1::BatchCheckItem *check = context->req.add_checks();
            check->set_correlation_id(std::to_string(item.params.request_id()));

            ::openfga::v1::CheckRequestTupleKey* tupleKey = check->mutable_tuple_key();
            fill_tuple_key(item.params.payload().tuple, tupleKey);
        }
        
        // Set deadline
        context->ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(config_.timeout_ms));

        auto callback = [context, items = std::move(items)](::grpc::Status status) mutable
        {
            if (status.ok())
            {
                const auto& result_map = context->res.result();
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
                            out.body.checkTuple.allow = res.allowed();
                            out.status = FGA_RESPONSE_OK;
                        }
                        else if (res.has_error())
                        {
                            out.body.checkTuple.allow = false;
                            out.status = FGA_RESPONSE_SERVER_ERROR;
                            strlcpy(out.error_message, res.error().message().c_str(), sizeof(out.error_message));
                        }
                        else
                        {
                            out.body.checkTuple.allow = false;
                            out.status = FGA_RESPONSE_CLIENT_ERROR;
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

        stub_->async()->BatchCheck(&context->ctx, &context->req, &context->res, std::move(callback));
    }


    void OpenFgaGrpcClient::handle_request(CheckTuple& req, ProcessCallback cb)
    {
        auto context = std::make_shared<CheckContext>();
        fill_check_request(config_, req, context->req);
        
        // Set deadline
        context->ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(config_.timeout_ms));

        auto callback = [context, &req, cb = std::move(cb)](::grpc::Status status) mutable
        {
            FgaResponse& res = req.response();
            if (status.ok())
            {
                res.body.checkTuple.allow = context->res.allowed();
                res.status = FGA_RESPONSE_OK;
            }
            else
            {
                res.body.checkTuple.allow = false;
                res.status = FGA_RESPONSE_CLIENT_ERROR;
                strlcpy(res.error_message, status.error_message().c_str(), sizeof(res.error_message));
            }
            cb();
        };

        stub_->async()->Check(&context->ctx, &context->req, &context->res, std::move(callback));
        // res.status = FGA_RESPONSE_OK;
        // cb();
    }

    void OpenFgaGrpcClient::handle_request(WriteTuple& req, ProcessCallback cb) {}
    void OpenFgaGrpcClient::handle_request(DeleteTuple& req, ProcessCallback cb) {}
    void OpenFgaGrpcClient::handle_request(GetStoreRequest& req, ProcessCallback cb) {}
    void OpenFgaGrpcClient::handle_request(CreateStoreRequest& req, ProcessCallback cb) {}
    void OpenFgaGrpcClient::handle_request(InvalidRequest& req, ProcessCallback cb) {}
} // namespace postfga::client
