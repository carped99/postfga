#include <postgres.h>


#include <fmgr.h>
#include <funcapi.h>
#include <utils/builtins.h>

#include "cache.h"
#include "channel.h"
#include "config.h"
#include "payload.h"

PG_FUNCTION_INFO_V1(postfga_check);
PG_FUNCTION_INFO_V1(postfga_write_tuple);
PG_FUNCTION_INFO_V1(postfga_delete_tuple);
PG_FUNCTION_INFO_V1(postfga_create_store);
PG_FUNCTION_INFO_V1(postfga_delete_store);

/*-------------------------------------------------------------------------
 * Static helpers
 *-------------------------------------------------------------------------
 */
static void write_tuple(const char* object_type,
                  const char* object_id,
                  const char* subject_type,
                  const char* subject_id,
                  const char* relation,
                  FgaTuple* out)
{
    strlcpy(out->object_type, object_type, sizeof(out->object_type));
    strlcpy(out->object_id, object_id, sizeof(out->object_id));
    strlcpy(out->subject_type, subject_type, sizeof(out->subject_type));
    strlcpy(out->subject_id, subject_id, sizeof(out->subject_id));
    strlcpy(out->relation, relation, sizeof(out->relation));
}

static bool fetch_check(
                const char* store_id,
                const char* auth_model_id,
                const char* object_type,
                  const char* object_id,
                  const char* subject_type,
                  const char* subject_id,
                  const char* relation,
                  bool* allowed_out)
{
    FgaRequest request = {0};
    FgaResponse response = {0};
    request.type = FGA_REQUEST_CHECK_TUPLE;
    strlcpy(request.store_id, store_id, sizeof(request.store_id));
    
    FgaCheckTupleRequest* payload = &request.body.checkTuple;
    write_tuple(object_type, object_id, subject_type, subject_id, relation, &payload->tuple);

    postfga_channel_execute(&request, &response);
    if (response.status == FGA_RESPONSE_OK)
    {
        *allowed_out = response.body.checkTuple.allow;
        return true;
    }

    ereport(INFO, (errmsg("postfga: check tuple failed - %s", response.error_message)));

    *allowed_out = false;
    return false;    
}

Datum postfga_check(PG_FUNCTION_ARGS)
{
    PostfgaConfig* config = postfga_get_config();
    uint64_t ttl_ms = config->cache_ttl_ms;
    const char* store_id = config->store_id;
    const char* auth_model_id = config->authorization_model_id;
    const char* object_type = text_to_cstring(PG_GETARG_TEXT_PP(0));
    const char* object_id = text_to_cstring(PG_GETARG_TEXT_PP(1));
    const char* subject_type = text_to_cstring(PG_GETARG_TEXT_PP(2));
    const char* subject_id = text_to_cstring(PG_GETARG_TEXT_PP(3));
    const char* relation = text_to_cstring(PG_GETARG_TEXT_PP(4));
    bool       allowed;

    FgaAclCacheKey key = postfga_cache_key(store_id,
                                           auth_model_id,
                                           object_type,
                                           object_id,
                                           subject_type,
                                           subject_id,
                                           relation);

    if (postfga_cache_lookup(&key, ttl_ms, &allowed))
    {
        PG_RETURN_BOOL(allowed);
    }

    if (!fetch_check(store_id,
                    auth_model_id,
                    object_type,
                    object_id,
                    subject_type,
                    subject_id,
                    relation,
                    &allowed))
    {
        // 오류 발생 시 기본적으로 거부
        PG_RETURN_BOOL(false);
    }

    postfga_cache_store(&key, ttl_ms, allowed);

    PG_RETURN_BOOL(allowed);
}

Datum postfga_write_tuple(PG_FUNCTION_ARGS)
{
    const char* object_type = text_to_cstring(PG_GETARG_TEXT_PP(0));
    const char* object_id = text_to_cstring(PG_GETARG_TEXT_PP(1));
    const char* subject_type = text_to_cstring(PG_GETARG_TEXT_PP(2));
    const char* subject_id = text_to_cstring(PG_GETARG_TEXT_PP(3));
    const char* relation = text_to_cstring(PG_GETARG_TEXT_PP(4));

    FgaRequest request;
    FgaResponse response;
    MemSet(&request, 0, sizeof(request));
    MemSet(&response, 0, sizeof(response));
    request.type = FGA_REQUEST_WRITE_TUPLE;
    write_tuple(object_type, object_id, subject_type, subject_id, relation, &request.body.writeTuple.tuple);

    postfga_channel_execute(&request, &response);
    if (response.status != FGA_RESPONSE_OK)
    {
        ereport(ERROR, errmsg("postfga: write tuple failed - %s", response.error_message));
    }
    PG_RETURN_BOOL(true);
}

Datum postfga_delete_tuple(PG_FUNCTION_ARGS)
{
    const char* object_type = text_to_cstring(PG_GETARG_TEXT_PP(0));
    const char* object_id = text_to_cstring(PG_GETARG_TEXT_PP(1));
    const char* subject_type = text_to_cstring(PG_GETARG_TEXT_PP(2));
    const char* subject_id = text_to_cstring(PG_GETARG_TEXT_PP(3));
    const char* relation = text_to_cstring(PG_GETARG_TEXT_PP(4));

    FgaRequest request;
    FgaResponse response;
    MemSet(&request, 0, sizeof(request));
    MemSet(&response, 0, sizeof(response));
    request.type = FGA_REQUEST_DELETE_TUPLE;
    write_tuple(object_type, object_id, subject_type, subject_id, relation, &request.body.deleteTuple.tuple);

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
    char       *store_name;
    FgaRequest  request;
    FgaResponse response;
    TupleDesc   tupdesc;
    Datum       values[2];
    bool        nulls[2];
    HeapTuple   tuple;

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