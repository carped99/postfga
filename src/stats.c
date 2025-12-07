/*-------------------------------------------------------------------------
 *
 * stats_api.c
 *    Statistics tracking implementation for PostFGA extension.
 *
 *-------------------------------------------------------------------------
 */
#include <postgres.h>

#include <miscadmin.h>
#include <storage/proc.h>
#include <storage/shmem.h>

#include "state.h"
#include "stats.h"


/*-------------------------------------------------------------------------
 * Shared memory
 *-------------------------------------------------------------------------*/
Size fga_stats_shmem_size()
{
    Size size = offsetof(FgaStats, backends);

    size = add_size(size, mul_size(sizeof(FgaBackendStats), MaxBackends));

    return size;
}

void fga_stats_shmem_init(FgaStats* stats)
{
    int i;

    MemSet(stats, 0, fga_stats_shmem_size());

    pg_atomic_init_u64(&stats->cache_entries, 0);
    pg_atomic_init_u64(&stats->cache_hits, 0);
    pg_atomic_init_u64(&stats->cache_misses, 0);
    pg_atomic_init_u64(&stats->cache_evictions, 0);
    pg_atomic_init_u64(&stats->bgw_wakeups, 0);
    pg_atomic_init_u64(&stats->requests_enqueued, 0);
    pg_atomic_init_u64(&stats->requests_processed, 0);

    /* 2) per-backend 슬롯 초기화 */
    for (i = 0; i < MaxBackends; i++)
        MemSet(&stats->backends[i], 0, sizeof(FgaBackendStats));
}

FgaBackendStats* fga_stats_get_backend(void)
{
    if (MyProcNumber == INVALID_PROC_NUMBER)
        return NULL;

    return &fga_get_stats()->backends[MyProcNumber];
}

static inline FgaBackendStats* backend_stats()
{
    if (MyProcNumber == INVALID_PROC_NUMBER)
        return NULL;

    return &fga_get_stats()->backends[MyProcNumber];
}

void fga_stats_l1_hit(void)
{
    FgaBackendStats* stats = backend_stats();
    if (stats)
        stats->l1_hits++;
}

void fga_stats_l1_miss(void)
{
    FgaBackendStats* stats = backend_stats();
    if (stats)
        stats->l1_misses++;
}

void fga_stats_l2_hit(void)
{
    FgaBackendStats* stats = backend_stats();
    if (stats)
        stats->l2_hits++;
}

void fga_stats_l2_miss(void)
{
    FgaBackendStats* stats = backend_stats();
    if (stats)
        stats->l2_misses++;
}
