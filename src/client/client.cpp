#include "client.hpp"

// 구체적 클라이언트 구현
#include "config/config.hpp"
#include "openfga_client.hpp"

namespace fga::client
{
    std::shared_ptr<Client> make_client(const fga::Config& cfg)
    {
        return std::make_shared<OpenFgaGrpcClient>(cfg);
    }

} // namespace fga::client
