// factory.hpp
#pragma once

#include <memory>

#include "client.hpp"
#include "config/config.hpp"

namespace postfga::client
{

    std::shared_ptr<Client> make_client(const Config& cfg);

} // namespace postfga::client
