#include "channel_factory.hpp"

namespace postfga::client
{

    std::shared_ptr<::grpc::Channel> make_channel(const postfga::Config& cfg)
    {
        grpc::ChannelArguments args;

        // Max message size 설정 (옵션)
        if (cfg.channel.max_message_size > 0)
        {
            args.SetMaxReceiveMessageSize(cfg.channel.max_message_size);
            args.SetMaxSendMessageSize(cfg.channel.max_message_size);
        }

        // keepalive 옵션 (필요할 때만)
        if (cfg.channel.keepalive_time_ms > 0)
        {
            args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, cfg.channel.keepalive_time_ms);
        }
        if (cfg.channel.keepalive_timeout_ms > 0)
        {
            args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, cfg.channel.keepalive_timeout_ms);
        }
        args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);

        // args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, cfg.channel.max_reconnect_backoff_ms);
        // args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, cfg.channel.min_reconnect_backoff_ms);

        std::shared_ptr<grpc::ChannelCredentials> creds;
        if (cfg.tls.use_tls)
        {
            grpc::SslCredentialsOptions ssl_opts;
            ssl_opts.pem_root_certs = cfg.tls.root_certs; // 필요 시
            ssl_opts.pem_private_key = cfg.tls.client_key;
            ssl_opts.pem_cert_chain = cfg.tls.client_cert;
            creds = grpc::SslCredentials(ssl_opts);
        }
        else
        {
            creds = grpc::InsecureChannelCredentials();
        }

        return grpc::CreateCustomChannel(cfg.endpoint, creds, args);
    }

} // namespace postfga::client
