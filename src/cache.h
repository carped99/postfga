#ifndef FGA_CACHE_H
#define FGA_CACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>

#include <storage/lwlock.h>
#include <utils/timestamp.h>

#include "postfga.h"

#ifdef __cplusplus
} /* extern "C" */
#endif

/* -------------------------------------------------------------------------
 * Cache data structures
 * ------------------------------------------------------------------------- */
typedef struct FgaAclCacheKey
{
    uint64_t low;         /* 8 bytes, offset 0 */
    uint64_t high;        /* 8 bytes, offset 8 */
    uint64_t object_key;  /* 8 bytes, offset 16 */
    uint32_t relation_id; /* 4 bytes, offset 24 (uint16→uint32로 변경) */
    uint32_t _pad;        /* 4 bytes, offset 28 */
} FgaAclCacheKey;

typedef struct FgaRelationCacheEntry
{
    char name[RELATION_MAX_LEN];
    uint16_t id;
} FgaRelationCacheEntry;

struct FgaL2AclCache;
typedef struct FgaL2AclCache FgaL2AclCache;

/* -------------------------------------------------------------------------
 * Cache API
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif

    Size fga_cache_shmem_base_size(void);
    Size fga_cache_shmem_hash_size(void);
    void fga_cache_shmem_init(FgaL2AclCache* cache, LWLock* lock);
    void fga_cache_shmem_each_startup(void);

    /* generation bump (invalidation) */
    // void postfga_l2_bump_generation(FgaL2AclCache* cache);

    void fga_cache_key(FgaAclCacheKey* key,
                       const char* store_id,
                       const char* model_id,
                       const text* object_type,
                       const text* object_id,
                       const text* subject_type,
                       const text* subject_id,
                       const text* relation);

    bool fga_cache_lookup(const FgaAclCacheKey* key, uint64_t ttl_ms, bool* allowed_out);

    void fga_cache_store(const FgaAclCacheKey* key, uint64_t ttl_ms, bool allowed);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FGA_CACHE_H */
