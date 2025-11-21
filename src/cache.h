/*-------------------------------------------------------------------------
 *
 * cache.h
 *    ACL cache operations for PostFGA extension.
 *
 *-------------------------------------------------------------------------
 */

#ifndef POSTFGA_CACHE_H
#define POSTFGA_CACHE_H

#include <postgres.h>
#include <datatype/timestamp.h>

#include "common.h"

#define NAME_MAX_LEN 64

/*
 * ACL Cache Key / Entry
 */
typedef struct AclCacheKey
{
    char object_type[OBJECT_TYPE_MAX_LEN];
    char object_id[OBJECT_ID_MAX_LEN];
    char subject_type[SUBJECT_TYPE_MAX_LEN];
    char subject_id[SUBJECT_ID_MAX_LEN];
} AclCacheKey;

typedef struct AclCacheEntry
{
    AclCacheKey  key;

    /* Generation counters for invalidation */
    uint64       object_type_gen;
    uint64       object_gen;
    uint64       subject_type_gen;
    uint64       subject_gen;

    /* Relation bitmasks */
    uint64       allow_bits;
    uint64       deny_bits;

    /* Timestamps */
    TimestampTz  last_updated;
    TimestampTz  expire_at;
} AclCacheEntry;

/* -------------------------------------------------------------------------
 * Cache operations
 * -------------------------------------------------------------------------
 */

/* Lookup cache entry by key */
bool cache_lookup(const AclCacheKey *key, AclCacheEntry *entry);

/* Insert or update cache entry */
void cache_insert(const AclCacheEntry *entry);

/* Delete cache entry by key */
void cache_delete(const AclCacheKey *key);

/* Check if cache entry is stale based on generation counters */
bool cache_is_stale(const AclCacheEntry *entry);

/* Invalidate cache by scope */
void cache_invalidate_by_scope(const char *scope_key);

#endif /* POSTFGA_CACHE_H */