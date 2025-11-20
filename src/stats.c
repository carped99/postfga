/*-------------------------------------------------------------------------
 *
 * stats.c
 *    Statistics tracking implementation for PostFGA extension.
 *
 *-------------------------------------------------------------------------
 */

#include <postgres.h>

#include "stats.h"

/*
 * stats_init
 *
 * Initialize all statistics counters to zero.
 */
void
stats_init(Stats *stats)
{
    if (!stats)
        return;

    pg_atomic_init_u64(&stats->cache_entries, 0);
    pg_atomic_init_u64(&stats->cache_hits, 0);
    pg_atomic_init_u64(&stats->cache_misses, 0);
    pg_atomic_init_u64(&stats->cache_evictions, 0);
    pg_atomic_init_u64(&stats->bgw_wakeups, 0);
    pg_atomic_init_u64(&stats->requests_enqueued, 0);
    pg_atomic_init_u64(&stats->requests_processed, 0);
}

/*
 * stats_inc_cache_hit
 *
 * Increment cache hit counter.
 */
void
stats_inc_cache_hit(Stats *stats)
{
    if (stats)
        pg_atomic_fetch_add_u64(&stats->cache_hits, 1);
}

/*
 * stats_inc_cache_miss
 *
 * Increment cache miss counter.
 */
void
stats_inc_cache_miss(Stats *stats)
{
    if (stats)
        pg_atomic_fetch_add_u64(&stats->cache_misses, 1);
}

/*
 * stats_inc_cache_entry
 *
 * Increment cache entry counter.
 */
void
stats_inc_cache_entry(Stats *stats)
{
    if (stats)
        pg_atomic_fetch_add_u64(&stats->cache_entries, 1);
}

/*
 * stats_dec_cache_entry
 *
 * Decrement cache entry counter (with underflow protection).
 */
void
stats_dec_cache_entry(Stats *stats)
{
    if (stats)
    {
        uint64 current = pg_atomic_read_u64(&stats->cache_entries);
        if (current > 0)
            pg_atomic_fetch_sub_u64(&stats->cache_entries, 1);
    }
}

/*
 * stats_inc_cache_eviction
 *
 * Increment cache eviction counter.
 */
void
stats_inc_cache_eviction(Stats *stats)
{
    if (stats)
        pg_atomic_fetch_add_u64(&stats->cache_evictions, 1);
}

/*
 * stats_inc_bgw_wakeup
 *
 * Increment BGW wakeup counter.
 */
void
stats_inc_bgw_wakeup(Stats *stats)
{
    if (stats)
        pg_atomic_fetch_add_u64(&stats->bgw_wakeups, 1);
}

/*
 * stats_inc_request_enqueued
 *
 * Increment request enqueued counter.
 */
void
stats_inc_request_enqueued(Stats *stats)
{
    if (stats)
        pg_atomic_fetch_add_u64(&stats->requests_enqueued, 1);
}

/*
 * stats_inc_request_processed
 *
 * Increment request processed counter.
 */
void
stats_inc_request_processed(Stats *stats)
{
    if (stats)
        pg_atomic_fetch_add_u64(&stats->requests_processed, 1);
}

/*
 * stats_get_cache_hits
 *
 * Get current cache hit count.
 */
uint64
stats_get_cache_hits(Stats *stats)
{
    return stats ? pg_atomic_read_u64(&stats->cache_hits) : 0;
}

/*
 * stats_get_cache_misses
 *
 * Get current cache miss count.
 */
uint64
stats_get_cache_misses(Stats *stats)
{
    return stats ? pg_atomic_read_u64(&stats->cache_misses) : 0;
}

/*
 * stats_get_cache_entries
 *
 * Get current cache entry count.
 */
uint64
stats_get_cache_entries(Stats *stats)
{
    return stats ? pg_atomic_read_u64(&stats->cache_entries) : 0;
}
