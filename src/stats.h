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

    typedef struct FgaBackendStats
    {
        uint64 check_calls;
        uint64 check_allowed;
        uint64 check_denied;
        uint64 check_error;

        uint64 l1_hits;
        uint64 l1_misses;
        uint64 l2_hits;
        uint64 l2_misses;

        uint64 rpc_check_calls;
        uint64 rpc_check_error;
        uint64 rpc_check_latency_sum_us;
    } FgaBackendStats;

    typedef struct FgaStats
    {
        pg_atomic_uint64 cache_entries;      /* Current cache entry count */
        pg_atomic_uint64 cache_hits;         /* Cache hit count */
        pg_atomic_uint64 cache_misses;       /* Cache miss count */
        pg_atomic_uint64 cache_evictions;    /* Cache eviction count */
        pg_atomic_uint64 bgw_wakeups;        /* BGW wakeup count */
        pg_atomic_uint64 requests_enqueued;  /* Requests enqueued count */
        pg_atomic_uint64 requests_processed; /* Requests processed count */
        FgaBackendStats backends[FLEXIBLE_ARRAY_MEMBER];
    } FgaStats;

    /* Initialize statistics */
    Size fga_stats_shmem_size();
    void fga_stats_shmem_init(FgaStats* stats);

    void fga_stats_l1_hit(void);
    void fga_stats_l1_miss(void);

    void fga_stats_l2_hit(void);
    void fga_stats_l2_miss(void);

    FgaBackendStats* fga_stats_get_backend(void);
#ifdef __cplusplus
}
#endif

#endif /* FGA_STATS_H */
