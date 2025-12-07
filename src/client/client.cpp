#include "client.hpp"

// 구체적 클라이언트 구현
#include "config/config.hpp"
#include "openfga_client.hpp"

namespace postfga::client
{
    std::shared_ptr<Client> make_client(const postfga::Config& cfg)
    {
        return std::make_shared<OpenFgaGrpcClient>(cfg);
    }

} // namespace postfga::client
