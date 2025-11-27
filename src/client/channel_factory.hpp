#pragma once

#include <memory>
#include <grpcpp/grpcpp.h>
#include "config/config.hpp"

namespace postfga::client
{

    std::shared_ptr<::grpc::Channel> make_channel(const postfga::Config &cfg);

} // namespace postfga::client