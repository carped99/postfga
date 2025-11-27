// factory.hpp
#pragma once

#include "config/config.hpp"

#include <memory>

#include "client.hpp"

namespace postfga::client
{

std::shared_ptr<Client> make_client(const Config& cfg);

} // namespace postfga::client
