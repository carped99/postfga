#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>

#include "config/config.hpp"

namespace fga::client
{

    std::shared_ptr<::grpc::Channel> make_channel(const fga::Config& cfg);

} // namespace fga::client
