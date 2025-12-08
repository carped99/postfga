
extern "C"
{
#include "config.h"
}

#include "config.hpp"

namespace fga
{
    Config load_config_from_guc()
    {
        FgaConfig* guc = fga_get_config();
        
        Config cfg;
        cfg.endpoint = guc->endpoint ? guc->endpoint : "";
        cfg.timeout = std::chrono::milliseconds(guc->cache_ttl_ms);
        // cfg.use_tls = false;
        return cfg;
    }
} // namespace fga
