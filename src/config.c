#include "config.h"

/* Global configuration */
static PostfgaConfig config = {0};

PostfgaConfig* postfga_get_config(void)
{
    return &config;
}
