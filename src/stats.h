/*-------------------------------------------------------------------------
 *
 * stats.h
 *    Statistics tracking for PostFGA extension.
 *
 *-------------------------------------------------------------------------
 */

#ifndef POSTFGA_STATS_H
#define POSTFGA_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <postgres.h>
#include <port/atomics.h>

/*
 * Statistics structure
 */
typedef struct Stats
{
    pg_atomic_uint64    cache_entries;      /* Current cache entry count */
    pg_atomic_uint64    cache_hits;         /* Cache hit count */
    pg_atomic_uint64    cache_misses;       /* Cache miss count */
    pg_atomic_uint64    cache_evictions;    /* Cache eviction count */
    pg_atomic_uint64    bgw_wakeups;        /* BGW wakeup count */
    pg_atomic_uint64    requests_enqueued;  /* Requests enqueued count */
    pg_atomic_uint64    requests_processed; /* Requests processed count */
} Stats;

/* Initialize statistics counters */
void stats_init(Stats *stats);

/* Increment cache hit counter */
void stats_inc_cache_hit(Stats *stats);

/* Increment cache miss counter */
void stats_inc_cache_miss(Stats *stats);

/* Increment/decrement cache entry counter */
void stats_inc_cache_entry(Stats *stats);
void stats_dec_cache_entry(Stats *stats);

/* Increment cache eviction counter */
void stats_inc_cache_eviction(Stats *stats);

/* Increment BGW wakeup counter */
void stats_inc_bgw_wakeup(Stats *stats);

/* Increment request counters */
void stats_inc_request_enqueued(Stats *stats);
void stats_inc_request_processed(Stats *stats);

/* Get statistics values */
uint64 stats_get_cache_hits(Stats *stats);
uint64 stats_get_cache_misses(Stats *stats);
uint64 stats_get_cache_entries(Stats *stats);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_STATS_H */