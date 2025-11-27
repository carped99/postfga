#include <postgres.h>
#include <fmgr.h>
#include <utils/builtins.h>

#include "channel.h"

PG_FUNCTION_INFO_V1(postfga_check);

Datum postfga_check(PG_FUNCTION_ARGS)
{
    const char *object_type = text_to_cstring(PG_GETARG_TEXT_PP(0));
    const char *object_id = text_to_cstring(PG_GETARG_TEXT_PP(1));
    const char *relation = text_to_cstring(PG_GETARG_TEXT_PP(2));
    const char *subject_type = text_to_cstring(PG_GETARG_TEXT_PP(3));
    const char *subject_id = text_to_cstring(PG_GETARG_TEXT_PP(4));

    bool allowed = postfga_channel_check(object_type, object_id, subject_type, subject_id, relation);

    PG_RETURN_BOOL(allowed);
}
