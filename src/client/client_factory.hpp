// factory.hpp
#pragma once

#include <memory>

#include "client.hpp"
#include "config/config.hpp"

namespace postfga::client
{

    std::unique_ptr<Client> make_client(const Config &cfg);

} // namespace postfga::client
