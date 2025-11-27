#include "client_factory.hpp"

// 구체적 클라이언트 구현
#include "openfga_client.hpp"

namespace postfga::client
{

    std::shared_ptr<Client> make_client(const Config& cfg)
    {
        return std::make_shared<OpenFgaGrpcClient>(cfg);
    }

} // namespace postfga::client
