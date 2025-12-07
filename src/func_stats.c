#include <postgres.h>

#include <fmgr.h>
#include <funcapi.h>
#include <utils/builtins.h>

#include "shmem.h"

PG_FUNCTION_INFO_V1(postfga_stats);

typedef struct FgaCacheComputedStats
{
    uint64 hits;
    uint64 misses;
    uint64 inserts;
    uint64 evictions;
    double hit_rate; /* 0.0 ~ 100.0 (%) */
} FgaCacheComputedStats;

static inline void fga_cache_compute_stats(FgaCacheStats* src, FgaCacheComputedStats* dst)
{
    uint64 lookups;
    dst->hits = pg_atomic_read_u64(&src->hits);
    dst->misses = pg_atomic_read_u64(&src->misses);

    dst->inserts = pg_atomic_read_u64(&src->inserts);
    dst->evictions = pg_atomic_read_u64(&src->evictions);

    lookups = dst->hits + dst->misses;
    if (lookups == 0)
        dst->hit_rate = 0.0;
    else
        dst->hit_rate = (double)dst->hits * 100.0 / (double)lookups;
}


Datum postfga_stats(PG_FUNCTION_ARGS)
{
    TupleDesc tupdesc;
    Datum values[7];
    bool nulls[7] = {false};
    FgaCacheComputedStats l1;
    FgaCacheComputedStats l2;
    FgaCacheComputedStats total;
    uint64 lookups;
    HeapTuple tuple;

    FgaStats* stats = &postfga_get_shmem_state()->stats;

    /* 한번에 snapshot */
    fga_cache_compute_stats(&stats->l1_cache, &l1);
    fga_cache_compute_stats(&stats->l2_cache, &l2);

    total.misses = l1.misses + l2.misses;
    total.hits = l1.hits + l2.hits;
    total.inserts = l1.inserts + l2.inserts;
    total.evictions = l1.evictions + l2.evictions;

    lookups = total.hits + total.misses;
    if (lookups == 0)
        total.hit_rate = 0.0;
    else
        total.hit_rate = (double)total.hits * 100.0 / (double)lookups;

    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("return type must be a row type")));

    BlessTupleDesc(tupdesc);
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, 0, sizeof(nulls));

    /* 컬럼 순서는 SQL에 맞춰서 */
    values[0] = UInt64GetDatum(l1.hits);
    values[1] = UInt64GetDatum(l1.misses);
    values[2] = Float8GetDatum(l1.hit_rate);

    values[4] = UInt64GetDatum(l2.hits);
    values[3] = UInt64GetDatum(l2.misses);
    values[5] = Float8GetDatum(l2.hit_rate);

    values[6] = Float8GetDatum(total.hit_rate); /* 전체 hit율 */

    tuple = heap_form_tuple(tupdesc, values, nulls);

    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}
