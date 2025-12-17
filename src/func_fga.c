#include <postgres.h>

#include <fmgr.h>
#include <funcapi.h>
#include <utils/builtins.h>
#include <utils/jsonb.h>

#include "cache.h"
#include "channel.h"
#include "config.h"
#include "payload.h"

PG_FUNCTION_INFO_V1(fga_check);
PG_FUNCTION_INFO_V1(fga_write_tuple);
PG_FUNCTION_INFO_V1(fga_delete_tuple);
PG_FUNCTION_INFO_V1(fga_create_store);
PG_FUNCTION_INFO_V1(fga_delete_store);

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

static inline void fill_tuple(TupleArgsView v, FgaTuple* tuple)
{
    write_text(tuple->object_type, sizeof(tuple->object_type), v.object_type);
    write_text(tuple->object_id, sizeof(tuple->object_id), v.object_id);
    write_text(tuple->subject_type, sizeof(tuple->subject_type), v.subject_type);
    write_text(tuple->subject_id, sizeof(tuple->subject_id), v.subject_id);
    write_text(tuple->relation, sizeof(tuple->relation), v.relation);
}

static inline void fill_request(FgaRequest* request)
{
    FgaConfig* config = fga_get_config();
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

static inline void build_cache_key(FgaAclCacheKey* key, const TupleArgsView* args)
{
    FgaConfig* config = fga_get_config();
    
    fga_cache_key(key,
                  config->store_id,
                  config->model_id,
                  args->object_type,
                  args->object_id,
                  args->subject_type,
                  args->subject_id,
                  args->relation);

}

static inline void validate_not_empty(text* arg, const char* argname)
{
    if (unlikely(arg == NULL))
    {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("postfga: %s argument must not be NULL", argname)));
    }

    if (VARSIZE_ANY_EXHDR(arg) == 0)
    {
        ereport(ERROR,
                (errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH), errmsg("postfga: %s argument must not be empty", argname)));
    }
}

static inline TupleArgsView read_tuple_args(FunctionCallInfo fcinfo)
{
    TupleArgsView v;
    v.object_type = PG_ARGISNULL(0) ? NULL : PG_GETARG_TEXT_PP(0);
    validate_not_empty(v.object_type, "object_type");

    v.object_id = PG_ARGISNULL(1) ? NULL : PG_GETARG_TEXT_PP(1);
    validate_not_empty(v.object_id, "object_id");

    v.subject_type = PG_ARGISNULL(2) ? NULL : PG_GETARG_TEXT_PP(2);
    validate_not_empty(v.subject_type, "subject_type");

    v.subject_id = PG_ARGISNULL(3) ? NULL : PG_GETARG_TEXT_PP(3);
    validate_not_empty(v.subject_id, "subject_id");

    v.relation = PG_ARGISNULL(4) ? NULL : PG_GETARG_TEXT_PP(4);
    validate_not_empty(v.relation, "relation");

    v.options = (PG_NARGS() >= 6 && !PG_ARGISNULL(5)) ? DatumGetJsonbP(PG_GETARG_DATUM(5)) : NULL;
    return v;
}

Datum fga_check(PG_FUNCTION_ARGS)
{
    bool allowed;
    TupleArgsView args = read_tuple_args(fcinfo);

    FgaAclCacheKey key;
    build_cache_key(&key, &args);

    if (fga_cache_lookup(&key, &allowed))
    {
        PG_RETURN_BOOL(allowed);
    }

    {
        FgaChannelSlot* slot = fga_channel_acquire_slot();
        FgaRequest* request = &slot->payload.request;
        FgaResponse* response = &slot->payload.response;
        request->type = FGA_REQUEST_CHECK;
        fill_request(request);
        fill_tuple(args, &request->body.checkTuple.tuple);

        fga_channel_execute_slot(slot);

        if (response->status == FGA_RESPONSE_OK)
        {
            allowed = response->body.checkTuple.allow;
        } else {
            ereport(INFO, (errmsg("postfga: check tuple failed - %s", response->error_message)));
        }

        fga_channel_release_slot(slot);
    }

    PG_RETURN_BOOL(allowed);
}

Datum fga_write_tuple(PG_FUNCTION_ARGS)
{
    TupleArgsView args = read_tuple_args(fcinfo);

    FgaChannelSlot* const slot = fga_channel_acquire_slot();
    FgaRequest* const request = &slot->payload.request;
    FgaResponse* const response = &slot->payload.response;

    PG_TRY();
    {
        request->type = FGA_REQUEST_WRITE_TUPLE;
        fill_request(request);
        fill_tuple(args, &request->body.writeTuple.tuple);

        fga_channel_execute_slot(slot);

        if (slot->payload.response.status != FGA_RESPONSE_OK)
        {
            ereport(ERROR, errmsg("postfga: write tuple failed - %s", slot->payload.response.error_message));
        }

        fga_channel_release_slot(slot);
        PG_RETURN_BOOL(true);
    }
    PG_CATCH();
    {
        fga_channel_release_slot(slot);
        PG_RE_THROW();
    }
    PG_END_TRY();
}

Datum fga_delete_tuple(PG_FUNCTION_ARGS)
{
    TupleArgsView args = read_tuple_args(fcinfo);

    FgaChannelSlot* const slot = fga_channel_acquire_slot();
    FgaRequest* const request = &slot->payload.request;
    FgaResponse* const response = &slot->payload.response;

    PG_TRY();
    {
        request->type = FGA_REQUEST_DELETE_TUPLE;
        fill_request(request);
        fill_tuple(args, &request->body.deleteTuple.tuple);

        fga_channel_execute_slot(slot);

        if (response->status != FGA_RESPONSE_OK)
        {
            ereport(ERROR, errmsg("postfga: delete tuple failed - %s", response->error_message));
        }

        fga_channel_release_slot(slot);
        PG_RETURN_BOOL(true);
    }
    PG_CATCH();
    {
        fga_channel_release_slot(slot);
        PG_RE_THROW();
    }
    PG_END_TRY();
}

Datum fga_create_store(PG_FUNCTION_ARGS)
{
    char* store_name;
    TupleDesc tupdesc;
    Datum values[2];
    bool nulls[2];
    HeapTuple tuple;

    // validate args
    if (PG_ARGISNULL(0))
        ereport(ERROR, errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("store name cannot be NULL"));

    store_name = text_to_cstring(PG_GETARG_TEXT_PP(0));

    // prepare
    FgaChannelSlot* const slot = fga_channel_acquire_slot();
    FgaRequest* const request = &slot->payload.request;
    FgaResponse* const response = &slot->payload.response;

    PG_TRY();
    {
        request->type = FGA_REQUEST_CREATE_STORE;
        strlcpy(request->body.createStore.name, store_name, sizeof(request->body.createStore.name));

        // execute
        fga_channel_execute_slot(slot);

        // check response
        if (response->status != FGA_RESPONSE_OK)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                    errmsg("postfga: create store failed: %s", store_name),
                    errdetail("%s", response->error_message)));
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
        values[0] = CStringGetTextDatum(response->body.createStore.id);

        /* name */
        values[1] = CStringGetTextDatum(response->body.createStore.name);

        fga_channel_release_slot(slot);

        tuple = heap_form_tuple(tupdesc, values, nulls);
        PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
    }
    PG_CATCH();
    {
        fga_channel_release_slot(slot);
        PG_RE_THROW();
    }
    PG_END_TRY();
}

Datum fga_delete_store(PG_FUNCTION_ARGS)
{
    const char* store_id = text_to_cstring(PG_GETARG_TEXT_PP(0));

    FgaChannelSlot* const slot = fga_channel_acquire_slot();
    FgaRequest* const request = &slot->payload.request;
    FgaResponse* const response = &slot->payload.response;

    PG_TRY();
    {
        request->type = FGA_REQUEST_DELETE_STORE;
        strlcpy(request->store_id, store_id, sizeof(request->store_id));

        // execute
        fga_channel_execute_slot(slot);
        // check response
        if (response->status != FGA_RESPONSE_OK)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                    errmsg("postfga: delete store failed: %s", store_id),
                    errdetail("%s", response->error_message)));
        }

        fga_channel_release_slot(slot);
        PG_RETURN_VOID();
    }
    PG_CATCH();
    {
        fga_channel_release_slot(slot);
        PG_RE_THROW();
    }
    PG_END_TRY();
}
