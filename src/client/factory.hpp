// factory.hpp
#pragma once

#include <memory>

#include "config.hpp"
#include "client.hpp"

namespace postfga::client {

    std::unique_ptr<Client> make_client(const Config& cfg);

} // namespace postfga::client
