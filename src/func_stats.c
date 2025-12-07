/*-------------------------------------------------------------------------
 * func_stats.c
 *    Statistics reporting for PostFGA
 *-------------------------------------------------------------------------
 */

#include <postgres.h>

#include <funcapi.h>
#include <miscadmin.h>
#include <utils/builtins.h>

#include "state.h"
#include "stats.h"

static void add_row(Tuplestorestate* tupstore, TupleDesc tupdesc, const char* section, const char* metric, int64 value)
{
    Datum values[3];
    bool nulls[3] = {false, false, false};

    values[0] = CStringGetTextDatum(section);
    values[1] = CStringGetTextDatum(metric);
    values[2] = Int64GetDatum(value);

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
}

static void backend_stats(Tuplestorestate* tupstore, TupleDesc tupdesc)
{
    FgaStats* stats = fga_get_stats();

    uint64 check_calls = 0, check_allowed = 0, check_denied = 0;
    uint64 rpc_calls = 0, rpc_errors = 0, rpc_latency_sum = 0;

    for (int i = 0; i < MaxBackends; i++)
    {
        FgaBackendStats* b = &stats->backends[i];

        check_calls += b->check_calls;
        check_allowed += b->check_allowed;
        check_denied += b->check_denied;
        rpc_calls += b->rpc_check_calls;
        rpc_errors += b->rpc_check_error;
        rpc_latency_sum += b->rpc_check_latency_sum_us;
    }

    add_row(tupstore, tupdesc, "check", "calls", check_calls);
    add_row(tupstore, tupdesc, "check", "allowed", check_allowed);
    add_row(tupstore, tupdesc, "check", "denied", check_denied);
    add_row(tupstore, tupdesc, "rpc", "calls", rpc_calls);
    add_row(tupstore, tupdesc, "rpc", "errors", rpc_errors);
    add_row(tupstore, tupdesc, "rpc", "latency_sum_us", rpc_latency_sum);
}

PG_FUNCTION_INFO_V1(postfga_stats);

Datum postfga_stats(PG_FUNCTION_ARGS)
{
    ReturnSetInfo* rsinfo = (ReturnSetInfo*)fcinfo->resultinfo;
    TupleDesc tupdesc;
    Tuplestorestate* tupstore;
    MemoryContext oldcontext;
    FgaStats* stats;

    /* 1) 이 함수가 SETOF를 받아들일 수 있는 컨텍스트인지 확인 */
    if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("set-valued function called in context that cannot accept a set")));

    if ((rsinfo->allowedModes & SFRM_Materialize) == 0)
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required")));

    /* 2) 반환 타입이 row type인지 확인 */
    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("return type must be a row type")));

    /* 3) tuplestore 초기화 (per-query memory) */
    oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);
    tupstore = tuplestore_begin_heap(true, false, work_mem);
    MemoryContextSwitchTo(oldcontext);

    rsinfo->returnMode = SFRM_Materialize;
    rsinfo->setResult = tupstore;
    rsinfo->setDesc = BlessTupleDesc(tupdesc);

    /* 4) backend별 통계 합산 */
    backend_stats(tupstore, tupdesc);

    return (Datum)0;
}
