#include <postgres.h>
#include <fmgr.h>
#include <utils/builtins.h>

#include "func_check.h"
#include "channel.h"

PG_FUNCTION_INFO_V1(postfga_check);

Datum postfga_check(PG_FUNCTION_ARGS)
{
    const char *object_type = text_to_cstring(PG_GETARG_TEXT_PP(0));
    const char *object_id = text_to_cstring(PG_GETARG_TEXT_PP(1));
    const char *relation = text_to_cstring(PG_GETARG_TEXT_PP(2));
    const char *subject_type = text_to_cstring(PG_GETARG_TEXT_PP(3));
    const char *subject_id = text_to_cstring(PG_GETARG_TEXT_PP(4));

    FgaRequest request;
    // memset(&request, 0, sizeof(request));
    // strncpy(request.store_id, "default", sizeof(request.store_id) - 1);
    // strncpy(request.tuple.object_type, object_type, sizeof(request.tuple.object_type) - 1);
    // strncpy(request.tuple.object_id, object_id, sizeof(request.tuple.object_id) - 1);
    // strncpy(request.tuple.relation, relation, sizeof(request.tuple.relation) - 1);
    // strncpy(request.tuple.subject_type, subject_type, sizeof(request.tuple.subject_type) - 1);
    // strncpy(request.tuple.subject_id, subject_id, sizeof(request.tuple.subject_id) - 1);

    bool allowed = postfga_channel_execute(&request);

    PG_RETURN_BOOL(allowed);
}
