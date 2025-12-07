
extern "C"
{
#include "config.h"
}

#include "load.hpp"

namespace postfga
{
    Config load_config_from_guc()
    {
        auto guc = fga_get_config();
        Config cfg;
        cfg.endpoint = guc->endpoint ? guc->endpoint : "";
        cfg.store_id = guc->store_id ? guc->store_id : "";
        cfg.model_id = guc->model_id ? guc->model_id : "";
        cfg.timeout = std::chrono::milliseconds(guc->cache_ttl_ms);

        // cfg.use_tls = false;
        return cfg;
    }
} // namespace postfga
