// cache.c (참고용, 필요한 부분만 발췌)

#include <postgres.h>
#include <storage/shmem.h>
#include <utils/hsearch.h>

#include "cache.h"
#include "relation.h"
#include "generation.h"
#include "common.h"

static void
init_hash(HTAB **htab,
          const char *name,
          int nelem,
          Size keysize,
          Size entrysize)
{
    HASHCTL ctl;

    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = keysize;
    ctl.entrysize = entrysize;

    *htab = ShmemInitHash(name,
                          nelem,
                          nelem,
                          &ctl,
                          HASH_ELEM | HASH_BLOBS);

    if (*htab == NULL)
        elog(ERROR, "PostFGA cache: failed to create hash '%s'", name);
}

Size postfga_cache_estimate_size(int max_cache_entries)
{
    Size size = 0;

    /* relation bitmap map */
    size = add_size(size,
                    hash_estimate_size(DEFAULT_RELATION_COUNT,
                                       sizeof(RelationBitMapEntry)));

    /* ACL cache */
    size = add_size(size,
                    hash_estimate_size(max_cache_entries,
                                       sizeof(AclCacheEntry)));

    /* Generation maps (object/subject 타입 & 값) */
    size = add_size(size,
                    hash_estimate_size(DEFAULT_GEN_MAP_SIZE,
                                       sizeof(GenerationEntry))); /* object_type */
    size = add_size(size,
                    hash_estimate_size(max_cache_entries,
                                       sizeof(GenerationEntry))); /* object */
    size = add_size(size,
                    hash_estimate_size(DEFAULT_GEN_MAP_SIZE,
                                       sizeof(GenerationEntry))); /* subject_type */
    size = add_size(size,
                    hash_estimate_size(max_cache_entries,
                                       sizeof(GenerationEntry))); /* subject */

    return size;
}

void postfga_init_cache(Cache *cache, int max_cache_entries)
{
    Assert(cache != NULL);

    memset(cache, 0, sizeof(Cache));

    /* Relation bitmap hash */
    init_hash(&cache->relation_bitmap_map,
              "PostFGA Relation Bitmap",
              DEFAULT_RELATION_COUNT,
              RELATION_MAX_LEN,
              sizeof(RelationBitMapEntry));

    /* ACL cache */
    init_hash(&cache->acl_cache,
              "PostFGA ACL Cache",
              max_cache_entries,
              sizeof(AclCacheKey),
              sizeof(AclCacheEntry));

    /* Generation maps */
    init_hash(&cache->object_type_gen_map,
              "PostFGA Object Type Gen",
              DEFAULT_GEN_MAP_SIZE,
              NAME_MAX_LEN * 2,
              sizeof(GenerationEntry));

    init_hash(&cache->object_gen_map,
              "PostFGA Object Gen",
              max_cache_entries,
              NAME_MAX_LEN * 2,
              sizeof(GenerationEntry));

    init_hash(&cache->subject_type_gen_map,
              "PostFGA Subject Type Gen",
              DEFAULT_GEN_MAP_SIZE,
              NAME_MAX_LEN * 2,
              sizeof(GenerationEntry));

    init_hash(&cache->subject_gen_map,
              "PostFGA Subject Gen",
              max_cache_entries,
              NAME_MAX_LEN * 2,
              sizeof(GenerationEntry));
}
