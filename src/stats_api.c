/*-------------------------------------------------------------------------
 *
 * stats_api.c
 *    Statistics tracking implementation for PostFGA extension.
 *
 *-------------------------------------------------------------------------
 */
#include <postgres.h>

#include "stats_api.h"
#include "stats_type.h"
#include "shmem.h"

/*-------------------------------------------------------------------------
 * Static helpers
 *-------------------------------------------------------------------------*/
static FgaStats *postfga_get_stats_state(void)
{
    // PostfgaShmemState *state = postfga_get_shmem_state();
    // return state ? &state->stats : NULL;
    return NULL;
}

/*
 * postfga_init_stats
 *
 * Initialize all statistics counters to zero.
 */
void postfga_init_stats()
{
    FgaStats *stats = postfga_get_stats_state();
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
void stats_inc_cache_hit()
{
    FgaStats *stats = postfga_get_stats_state();
    if (stats)
        pg_atomic_fetch_add_u64(&stats->cache_hits, 1);
}

/*
 * stats_inc_cache_miss
 *
 * Increment cache miss counter.
 */
void stats_inc_cache_miss()
{
    FgaStats *stats = postfga_get_stats_state();
    if (stats)
        pg_atomic_fetch_add_u64(&stats->cache_misses, 1);
}

/*
 * stats_inc_cache_entry
 *
 * Increment cache entry counter.
 */
void stats_inc_cache_entry()
{
    FgaStats *stats = postfga_get_stats_state();
    if (stats)
        pg_atomic_fetch_add_u64(&stats->cache_entries, 1);
}

/*
 * stats_dec_cache_entry
 *
 * Decrement cache entry counter (with underflow protection).
 */
void stats_dec_cache_entry()
{
    FgaStats *stats = postfga_get_stats_state();
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
void stats_inc_cache_eviction()
{
    FgaStats *stats = postfga_get_stats_state();
    if (stats)
        pg_atomic_fetch_add_u64(&stats->cache_evictions, 1);
}

/*
 * stats_inc_bgw_wakeup
 *
 * Increment BGW wakeup counter.
 */
void stats_inc_bgw_wakeup()
{
    FgaStats *stats = postfga_get_stats_state();
    if (stats)
        pg_atomic_fetch_add_u64(&stats->bgw_wakeups, 1);
}

/*
 * stats_inc_request_enqueued
 *
 * Increment request enqueued counter.
 */
void stats_inc_request_enqueued()
{
    FgaStats *stats = postfga_get_stats_state();
    if (stats)
        pg_atomic_fetch_add_u64(&stats->requests_enqueued, 1);
}

/*
 * stats_inc_request_processed
 *
 * Increment request processed counter.
 */
void stats_inc_request_processed()
{
    FgaStats *stats = postfga_get_stats_state();
    if (stats)
        pg_atomic_fetch_add_u64(&stats->requests_processed, 1);
}

/*
 * stats_get_cache_hits
 *
 * Get current cache hit count.
 */
uint64_t
stats_get_cache_hits()
{
    FgaStats *stats = postfga_get_stats_state();
    return stats ? pg_atomic_read_u64(&stats->cache_hits) : 0;
}

/*
 * stats_get_cache_misses
 *
 * Get current cache miss count.
 */
uint64_t
stats_get_cache_misses()
{
    FgaStats *stats = postfga_get_stats_state();
    return stats ? pg_atomic_read_u64(&stats->cache_misses) : 0;
}

/*
 * stats_get_cache_entries
 *
 * Get current cache entry count.
 */
uint64_t
stats_get_cache_entries()
{
    FgaStats *stats = postfga_get_stats_state();
    return stats ? pg_atomic_read_u64(&stats->cache_entries) : 0;
}
