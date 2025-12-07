#ifndef FGA_STATS_TYPE_H
#define FGA_STATS_TYPE_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <postgres.h>

#include <port/atomics.h>

#include "cache.h"
#ifdef __cplusplus
}
#endif

/*
 * Statistics structure
 */
typedef struct FgaStats
{
    FgaCacheStats l1_cache;
    FgaCacheStats l2_cache;

    pg_atomic_uint64 cache_entries;      /* Current cache entry count */
    pg_atomic_uint64 cache_hits;         /* Cache hit count */
    pg_atomic_uint64 cache_misses;       /* Cache miss count */
    pg_atomic_uint64 cache_evictions;    /* Cache eviction count */
    pg_atomic_uint64 bgw_wakeups;        /* BGW wakeup count */
    pg_atomic_uint64 requests_enqueued;  /* Requests enqueued count */
    pg_atomic_uint64 requests_processed; /* Requests processed count */
} FgaStats;

#endif /* FGA_STATS_TYPE_H */
