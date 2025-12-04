#include <postgres.h>
#include <storage/shmem.h>

#include "cache_shmem.h"
#include "config.h"

/*-------------------------------------------------------------------------
 * Postfga Cache shared memory initialization
 *
 *    - Estimate shared memory size needed for cache
 *    - Initialize shared memory cache structures
 *-------------------------------------------------------------------------*/

static HTAB* init_acl_cache(void);

Size postfga_cache_shmem_size(void)
{
    PostfgaConfig* cfg = postfga_get_config();

    Size size = hash_estimate_size(cfg->max_cache_entries, sizeof(FgaAclCacheEntry));

    return size;
}

void postfga_cache_shmem_init(FgaL2Cache* cache, LWLock *lock)
{
    PostfgaConfig* cfg = postfga_get_config();
    Size shmem_size;

    Assert(cache != NULL);
    Assert(lock != NULL);

    /* initialize cache struct */
    MemSet(cache, 0, sizeof(*cache));
    cache->lock = lock;
    cache->generation = 0;

    cache->acl = init_acl_cache();
    if (cache->acl == NULL)
        ereport(FATAL,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("postfga: could not initialize L2 cache")));

    // MemSet(&ctl, 0, sizeof(ctl));
    // ctl.entrysize = sizeof(FgaRelationCacheEntry);
    // cache->relation = ShmemInitHash("PostFGA Relation cache",
    //                                 cfg->max_relations, /* init_size */
    //                                 cfg->max_relations, /* max_size  */
    //                                 &ctl,
    //                                 HASH_ELEM | HASH_STRINGS | HASH_SHARED_MEM);

    /* 초기화 정보 로그 */
    shmem_size = postfga_cache_shmem_size();
    
    ereport(LOG,
            (errmsg("postfga: L2 cache initialized: %d entries (%.2f MB)",
                    cfg->max_cache_entries,
                    (double)shmem_size / (1024.0 * 1024.0))));    
}


static HTAB* init_acl_cache(void)
{
    PostfgaConfig* cfg = postfga_get_config();
    HASHCTL ctl;

    /* initialize hash tables */
    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(FgaAclCacheKey);
    ctl.entrysize = sizeof(FgaAclCacheEntry);

    // ctl.hash = FgaAclCacheKeyHash;

    return ShmemInitHash("postfga L2 cache",
                            cfg->max_cache_entries, /* initial size */
                            cfg->max_cache_entries, /* max size  */
                            &ctl,
                            HASH_ELEM | HASH_BLOBS);
}