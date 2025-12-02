#include "openfga_client.hpp"
#include "payload.h"
#include "request_variant.hpp"
#include "util/logger.hpp"

namespace postfga::client
{
    namespace
    {
        void fill_tuple_key(const FgaTuple& tuple, ::openfga::v1::TupleKey* tuple_key)
        {
            tuple_key->set_object(std::string(tuple.object_type) + ":" + tuple.object_id);
            tuple_key->set_user(std::string(tuple.subject_type) + ":" + tuple.subject_id);
            tuple_key->set_relation(tuple.relation);
        }

        void fill_request(const Config &config, const WriteTuple& in, ::openfga::v1::WriteRequest& out)
        {
            const FgaWriteTupleRequest& payload = in.request();
            if (payload.store_id[0] != '\0')
                out.set_store_id(payload.store_id);
            else
                out.set_store_id(config.store_id);

            ::openfga::v1::WriteRequestWrites* writes = out.mutable_writes();
            writes->set_on_duplicate("ignore");

            ::openfga::v1::TupleKey* tuple_key = writes->add_tuple_keys();
            const FgaTuple& tuple = payload.tuple;
            fill_tuple_key(tuple, tuple_key);
        }

        void fill_request(const Config &config, const DeleteTuple& in, ::openfga::v1::WriteRequest& out)
        {
            const FgaDeleteTupleRequest& payload = in.request();
            if (payload.store_id[0] != '\0')
                out.set_store_id(payload.store_id);
            else
                out.set_store_id(config.store_id);

            ::openfga::v1::WriteRequestDeletes* deletes = out.mutable_deletes();
            deletes->set_on_missing("ignore");

            ::openfga::v1::TupleKeyWithoutCondition* tuple_key = deletes->add_tuple_keys();
            const FgaTuple& tuple = payload.tuple;
            tuple_key->set_object(std::string(tuple.object_type) + ":" + tuple.object_id);
            tuple_key->set_user(std::string(tuple.subject_type) + ":" + tuple.subject_id);
            tuple_key->set_relation(tuple.relation);
        }

        struct WriteContext
        {
            ::grpc::ClientContext context;
            ::openfga::v1::WriteRequest request;
            ::openfga::v1::WriteResponse response;
        };
    } // anonymous namespace

    void OpenFgaGrpcClient::handle_request(WriteTuple& req, ProcessCallback cb)
    {
        auto ctx = std::make_shared<WriteContext>();
        fill_request(config_, req, ctx->request);
        
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
                // res.body.checkTuple.allow = false;
                res.status = FGA_RESPONSE_CLIENT_ERROR;
                strlcpy(res.error_message, status.error_message().c_str(), sizeof(res.error_message));
            }
            cb();
        };

        stub_->async()->Write(&ctx->context, &ctx->request, &ctx->response, std::move(callback));
    }

    void OpenFgaGrpcClient::handle_request(DeleteTuple& req, ProcessCallback cb)
    {
        auto ctx = std::make_shared<WriteContext>();
        fill_request(config_, req, ctx->request);
        
        // Set deadline
        ctx->context.set_deadline(std::chrono::system_clock::now() + config_.timeout);
        
        auto callback = [ctx, req, cb = std::move(cb)](::grpc::Status status)
        {
            FgaResponse& res = req.response();
            if (status.ok())
            {
                res.status = FGA_RESPONSE_OK;
                res.body.deleteTuple.success = true;
            }
            else
            {
                res.status = FGA_RESPONSE_CLIENT_ERROR;
                strlcpy(res.error_message, status.error_message().c_str(), sizeof(res.error_message));
            }
            cb();
        };

        stub_->async()->Write(&ctx->context, &ctx->request, &ctx->response, std::move(callback));
    }
} // namespace postfga::client
