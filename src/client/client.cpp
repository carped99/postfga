#include "client.hpp"

namespace postfga::client {

ClientAdapter::~ClientAdapter()
{
    if (client_ != nullptr)
    {
        postfga_client_fini(client_);
        client_ = nullptr;
        elog(LOG, "PostFGA BGW: gRPC client finalized");
    }
}

bool ClientAdapter::init_from_guc()
{
    const char *endpoint = GetConfigOption("postfga.endpoint", false, false);

    if (endpoint == nullptr || endpoint[0] == '\0')
    {
        elog(WARNING, "PostFGA BGW: No endpoint configured (postfga.endpoint)");
        return false;
    }

    // 기존 클라이언트가 있으면 정리
    if (client_ != nullptr)
    {
        postfga_client_fini(client_);
        client_ = nullptr;
    }

    client_ = postfga_client_init(endpoint);
    if (client_ == nullptr)
    {
        elog(WARNING, "PostFGA BGW: Failed to initialize gRPC client (endpoint=%s)", endpoint);
        return false;
    }

    elog(LOG, "PostFGA BGW: gRPC client initialized with endpoint: %s", endpoint);
    return true;
}

bool ClientAdapter::ensure_initialized()
{
    if (client_ == nullptr)
    {
        if (!init_from_guc())
            return false;
    }

    if (!postfga_client_is_healthy(client_))
    {
        elog(WARNING, "PostFGA BGW: gRPC client is not healthy");

        /*
         * 필요하면 여기서 재시도 로직:
         *  - endpoint 바뀌었을 수 있으니 다시 init_from_guc() 시도
         *  - 실패하면 false 반환
         */
        if (!init_from_guc())
            return false;

        if (!postfga_client_is_healthy(client_))
        {
            elog(WARNING, "PostFGA BGW: gRPC client still unhealthy after re-init");
            return false;
        }
    }

    return true;
}

bool ClientAdapter::is_healthy() const
{
    if (client_ == nullptr)
        return false;

    return postfga_client_is_healthy(client_);
}

bool ClientAdapter::check_async(const GrpcCheckRequest &req,
                                CheckCallback callback,
                                void *user_data)
{
    if (client_ == nullptr)
    {
        elog(WARNING, "PostFGA BGW: check_async called before client initialized");
        return false;
    }

    if (callback == nullptr)
    {
        elog(WARNING, "PostFGA BGW: check_async called with null callback");
        return false;
    }

    if (!postfga_client_check_async(client_, &req, callback, user_data))
    {
        elog(WARNING, "PostFGA BGW: postfga_client_check_async() failed");
        return false;
    }

    return true;
}

bool ClientAdapter::write_async(const GrpcWriteRequest &req,
                                WriteCallback callback,
                                void *user_data)
{
    if (client_ == nullptr)
    {
        elog(WARNING, "PostFGA BGW: write_async called before client initialized");
        return false;
    }

    if (callback == nullptr)
    {
        elog(WARNING, "PostFGA BGW: write_async called with null callback");
        return false;
    }

    if (!postfga_client_write_async(client_, &req, callback, user_data))
    {
        elog(WARNING, "PostFGA BGW: postfga_client_write_async() failed");
        return false;
    }

    return true;
}

void ClientAdapter::poll(int timeout_ms)
{
    if (client_ == nullptr)
        return;

    postfga_client_poll(client_, timeout_ms);
}

void ClientAdapter::reset()
{
    if (client_ != nullptr)
    {
        postfga_client_fini(client_);
        client_ = nullptr;
        elog(LOG, "PostFGA BGW: gRPC client reset");
    }
}

} // namespace postfga
