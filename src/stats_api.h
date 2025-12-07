/*-------------------------------------------------------------------------
 *
 * stats_api.h
 *    Statistics tracking for PostFGA extension.
 *
 *-------------------------------------------------------------------------
 */

#ifndef FGA_STATS_H
#define FGA_STATS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Initialize statistics counters */
    void fga_stats_shmem_init();

    /* Increment cache hit counter */
    void stats_inc_cache_hit();

    /* Increment cache miss counter */
    void stats_inc_cache_miss();

    /* Increment/decrement cache entry counter */
    void stats_inc_cache_entry();
    void stats_dec_cache_entry();
    /* Increment cache eviction counter */
    void stats_inc_cache_eviction();

    /* Increment BGW wakeup counter */
    void stats_inc_bgw_wakeup();

    /* Increment request counters */
    void stats_inc_request_enqueued();
    void stats_inc_request_processed();
    /* Get statistics values */
    uint64_t stats_get_cache_hits();
    uint64_t stats_get_cache_misses();
    uint64_t stats_get_cache_entries();
#ifdef __cplusplus
}
#endif

#endif /* FGA_STATS_H */