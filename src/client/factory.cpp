#include "factory.hpp"
#include "config.hpp"
#include "client.hpp"

// 구체적 클라이언트 구현
#include "openfga.hpp"

namespace postfga::client {

std::unique_ptr<Client> make_client(const Config& cfg) {
    return std::make_unique<OpenFgaGrpcClient>(cfg);
}

} // namespace postfga::client