#include <postgres.h>

#include <fmgr.h>
#include <funcapi.h>
#include <utils/builtins.h>
#include <utils/jsonb.h>

#include "cache.h"
#include "channel.h"
#include "config.h"
#include "payload.h"

PG_FUNCTION_INFO_V1(postfga_check);
PG_FUNCTION_INFO_V1(postfga_write_tuple);
PG_FUNCTION_INFO_V1(postfga_delete_tuple);
PG_FUNCTION_INFO_V1(postfga_create_store);
PG_FUNCTION_INFO_V1(postfga_delete_store);

typedef struct TupleArgsView
{
    text* object_type;
    text* object_id;
    text* subject_type;
    text* subject_id;
    text* relation;
    Jsonb* options;
} TupleArgsView;

/*-------------------------------------------------------------------------
 * Static helpers
 *-------------------------------------------------------------------------
 */
static inline void write_text(char* dst, size_t dst_size, const text* src)
{
    size_t len;

    Assert(dst != NULL);
    Assert(src != NULL);
    Assert(dst_size > 0);

    /* varlena 길이 (헤더 제외) */
    len = VARSIZE_ANY_EXHDR(src);

    /* 너무 길면 잘라서 저장 (마지막은 NUL) */
    if (len >= dst_size)
        len = dst_size - 1;

    if (len > 0)
        memcpy(dst, VARDATA_ANY(src), len);

    dst[len] = '\0';
}

static inline void fill_tuple_args(TupleArgsView v, FgaTuple* tuple)
{
    write_text(tuple->object_type, sizeof(tuple->object_type), v.object_type);
    write_text(tuple->object_id, sizeof(tuple->object_id), v.object_id);
    write_text(tuple->subject_type, sizeof(tuple->subject_type), v.subject_type);
    write_text(tuple->subject_id, sizeof(tuple->subject_id), v.subject_id);
    write_text(tuple->relation, sizeof(tuple->relation), v.relation);
}

static inline void fill_request(FgaRequest* request)
{
    PostfgaConfig* config = postfga_get_config();
    if (config->store_id == NULL || config->store_id[0] == '\0')
    {
        ereport(ERROR, errmsg("postfga: store_id is not configured"));
    }
    strlcpy(request->store_id, config->store_id, sizeof(request->store_id));

    if (config->model_id != NULL)
    {
        strlcpy(request->model_id, config->model_id, sizeof(request->model_id));
    }
}

static inline void validate_not_empty(text* arg, const char* argname)
{
    if (unlikely(arg == NULL))
    {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("%s argument must not be NULL", argname)));
    }

    if (VARSIZE_ANY_EXHDR(arg) == 0)
    {
        ereport(ERROR,
                (errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH), errmsg("%s argument must not be empty", argname)));
    }
}

static inline TupleArgsView read_tuple_args(FunctionCallInfo fcinfo)
{
    TupleArgsView v;
    v.object_type = PG_ARGISNULL(0) ? NULL : PG_GETARG_TEXT_PP(0);
    v.object_id = PG_ARGISNULL(1) ? NULL : PG_GETARG_TEXT_PP(1);
    v.subject_type = PG_ARGISNULL(2) ? NULL : PG_GETARG_TEXT_PP(2);
    v.subject_id = PG_ARGISNULL(3) ? NULL : PG_GETARG_TEXT_PP(3);
    v.relation = PG_ARGISNULL(4) ? NULL : PG_GETARG_TEXT_PP(4);
    v.options = (PG_NARGS() >= 6 && !PG_ARGISNULL(5)) ? DatumGetJsonbP(PG_GETARG_DATUM(5)) : NULL;

    validate_not_empty(v.object_type, "object_type");
    validate_not_empty(v.object_id, "object_id");
    validate_not_empty(v.subject_type, "subject_type");
    validate_not_empty(v.subject_id, "subject_id");
    validate_not_empty(v.relation, "relation");

    return v;
}

Datum postfga_check(PG_FUNCTION_ARGS)
{
    TupleArgsView args = read_tuple_args(fcinfo);
    PostfgaConfig* config = postfga_get_config();
    uint64_t ttl_ms = config->cache_ttl_ms;
    bool allowed;

    FgaAclCacheKey key;
    postfga_cache_key(&key,
                      config->store_id,
                      config->model_id,
                      args.object_type,
                      args.object_id,
                      args.subject_type,
                      args.subject_id,
                      args.relation);

    if (postfga_cache_lookup(&key, ttl_ms, &allowed))
    {
        PG_RETURN_BOOL(allowed);
    }

    {
        FgaRequest request = {0};
        FgaResponse response = {0};
        request.type = FGA_REQUEST_CHECK_TUPLE;
        fill_request(&request);
        fill_tuple_args(args, &request.body.checkTuple.tuple);

        postfga_channel_execute(&request, &response);
        if (response.status == FGA_RESPONSE_OK)
        {
            postfga_cache_store(&key, ttl_ms, response.body.checkTuple.allow);
            PG_RETURN_BOOL(response.body.checkTuple.allow);
        }

        ereport(INFO, (errmsg("postfga: check tuple failed - %s", response.error_message)));
    }

    PG_RETURN_BOOL(false);
}

Datum postfga_write_tuple(PG_FUNCTION_ARGS)
{
    TupleArgsView args = read_tuple_args(fcinfo);

    FgaRequest request = {0};
    FgaResponse response = {0};
    request.type = FGA_REQUEST_WRITE_TUPLE;

    fill_tuple_args(args, &request.body.writeTuple.tuple);
    fill_request(&request);

    postfga_channel_execute(&request, &response);
    if (response.status != FGA_RESPONSE_OK)
    {
        ereport(ERROR, errmsg("postfga: write tuple failed - %s", response.error_message));
    }
    PG_RETURN_BOOL(true);
}

Datum postfga_delete_tuple(PG_FUNCTION_ARGS)
{
    TupleArgsView args = read_tuple_args(fcinfo);

    FgaRequest request = {0};
    FgaResponse response = {0};
    request.type = FGA_REQUEST_DELETE_TUPLE;

    fill_request(&request);
    fill_tuple_args(args, &request.body.deleteTuple.tuple);

    postfga_channel_execute(&request, &response);
    if (response.status == FGA_RESPONSE_OK)
    {
        PG_RETURN_BOOL(true);
    }
    ereport(INFO, errmsg("postfga: delete tuple failed - %s", response.error_message));
    PG_RETURN_BOOL(false);
}

Datum postfga_create_store(PG_FUNCTION_ARGS)
{
    char* store_name;
    FgaRequest request;
    FgaResponse response;
    TupleDesc tupdesc;
    Datum values[2];
    bool nulls[2];
    HeapTuple tuple;

    // validate args
    if (PG_ARGISNULL(0))
        ereport(ERROR, errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("store name cannot be NULL"));

    store_name = text_to_cstring(PG_GETARG_TEXT_PP(0));

    // prepare
    MemSet(&request, 0, sizeof(request));
    MemSet(&response, 0, sizeof(response));
    request.type = FGA_REQUEST_CREATE_STORE;
    strlcpy(request.body.createStore.name, store_name, sizeof(request.body.createStore.name));

    // execute
    postfga_channel_execute(&request, &response);

    // check response
    if (response.status != FGA_RESPONSE_OK)
    {
        ereport(ERROR,
                (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                 errmsg("postfga: create store failed: %s", store_name),
                 errdetail("%s", response.error_message)));
    }

    // build return tuple
    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("function returning record called in context that cannot accept type record")));

    tupdesc = BlessTupleDesc(tupdesc);

    /* 결과 튜플 구성 */
    MemSet(nulls, false, sizeof(nulls));

    /* id */
    values[0] = CStringGetTextDatum(response.body.createStore.id);

    /* name */
    values[1] = CStringGetTextDatum(response.body.createStore.name);

    tuple = heap_form_tuple(tupdesc, values, nulls);
    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

Datum postfga_delete_store(PG_FUNCTION_ARGS)
{
    const char* store_id = text_to_cstring(PG_GETARG_TEXT_PP(0));

    FgaRequest request = {0};
    FgaResponse response = {0};
    request.type = FGA_REQUEST_DELETE_STORE;
    strlcpy(request.store_id, store_id, sizeof(request.store_id));

    postfga_channel_execute(&request, &response);
    if (response.status != FGA_RESPONSE_OK)
    {
        ereport(ERROR,
                (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                 errmsg("postfga: delete store failed: %s", store_id),
                 errdetail("%s", response.error_message)));
    }
    PG_RETURN_VOID();
}
