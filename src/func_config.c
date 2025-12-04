#include <postgres.h>
#include <fmgr.h>
#include <funcapi.h>
#include <utils/builtins.h>

#include "config.h"

PG_FUNCTION_INFO_V1(postfga_config);

Datum postfga_config(PG_FUNCTION_ARGS)
{
    TupleDesc       tupdesc;
    Datum           values[11];
    bool            nulls[11];
    HeapTuple       tuple;
    PostfgaConfig* cfg = postfga_get_config();

    /* 결과 타입이 composite인지 확인 (RETURNS TABLE(...) or RETURNS postfga_config_type) */
    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("return type must be a row type")));

    BlessTupleDesc(tupdesc);

    MemSet(values, 0, sizeof(values));
    MemSet(nulls, 0, sizeof(nulls));

    int i = 0;

    /* 1) endpoint text */
    if (cfg->endpoint && cfg->endpoint[0] != '\0')
        values[i++] = CStringGetTextDatum(cfg->endpoint);
    else
        nulls[i++] = true;

    /* 2) store_id text */
    if (cfg->store_id && cfg->store_id[0] != '\0')
        values[i++] = CStringGetTextDatum(cfg->store_id);
    else
        nulls[i++] = true;

    /* 3) authorization_model_id text */
    if (cfg->authorization_model_id && cfg->authorization_model_id[0] != '\0')
        values[i++] = CStringGetTextDatum(cfg->authorization_model_id);
    else
        nulls[i++] = true;

    /* 4) cache_ttl_ms int4 */
    values[i++] = Int32GetDatum(cfg->cache_ttl_ms);

    /* 5) max_cache_entries int4 */
    values[i++] = Int32GetDatum(cfg->max_cache_entries);

    /* 7) fallback_to_grpc_on_miss bool */
    values[i++] = BoolGetDatum(cfg->fallback_to_grpc_on_miss);

    /* 8) cache_enabled bool */
    values[i++] = BoolGetDatum(cfg->cache_enabled);

    /* 9) cache_size int4 */
    values[i++] = Int32GetDatum(cfg->cache_size);

    /* 10) max_slots int4 */
    values[i++] = Int32GetDatum(cfg->max_slots);

    /* 11) max_relations int4 */
    values[i++] = Int32GetDatum(cfg->max_relations);

    tuple = heap_form_tuple(tupdesc, values, nulls);

    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}
