#include <postgres.h>
#include <fmgr.h>
#include "config.h"

PG_FUNCTION_INFO_V1(postfga_config);

Datum postfga_config(PG_FUNCTION_ARGS)
{
    PostfgaConfig* cfg = postfga_get_config();

    PG_RETURN_POINTER(cfg);
}
