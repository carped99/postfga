/*-------------------------------------------------------------------------
 *
 * cache.c
 *    ACL cache operations for PostFGA extension.
 *
 * This module implements:
 *   - Cache lookup, insert, delete operations
 *   - Cache staleness checking based on generation counters
 *   - Cache invalidation by scope
 *
 *-------------------------------------------------------------------------
 */

#include <postgres.h>

#include <storage/lwlock.h>
#include <utils/hsearch.h>
#include <string.h>

#include "cache.h"
#include "state.h"
#include "generation.h"
#include "stats.h"

/* -------------------------------------------------------------------------
 * Cache Operations
 * -------------------------------------------------------------------------
 */

/*
 * cache_lookup
 *
 * Lookup cache entry by key.
 *
 * Args:
 *   key   - Cache key to lookup
 *   entry - Output parameter for cache entry (if found)
 *
 * Returns: true if entry found, false otherwise
 *
 * Thread safety: Acquires shared lock on shared state lock
 */
bool
cache_lookup(const AclCacheKey *key, AclCacheEntry *entry)
{
    PostfgaShmemState *shared_state;
    AclCacheEntry *found_entry;
    bool result = false;

    shared_state = get_shared_state();
    if (!shared_state || !shared_state->acl_cache)
    {
        elog(WARNING, "PostFGA: Cache lookup called before initialization");
        return false;
    }

    if (!key || !entry)
    {
        elog(WARNING, "PostFGA: Invalid parameters for cache lookup");
        return false;
    }

    LWLockAcquire(shared_state->lock, LW_SHARED);

    found_entry = (AclCacheEntry *) hash_search(shared_state->acl_cache,
                                                key,
                                                HASH_FIND,
                                                NULL);

    if (found_entry)
    {
        memcpy(entry, found_entry, sizeof(AclCacheEntry));
        result = true;
        stats_inc_cache_hit(&shared_state->stats);
    }
    else
    {
        stats_inc_cache_miss(&shared_state->stats);
    }

    LWLockRelease(shared_state->lock);

    return result;
}

/*
 * cache_insert
 *
 * Insert or update cache entry.
 *
 * Args:
 *   entry - Cache entry to insert/update
 *
 * Thread safety: Acquires exclusive lock on shared state lock
 *
 * Note: If entry already exists, it will be overwritten.
 */
void
cache_insert(const AclCacheEntry *entry)
{
    PostfgaShmemState *shared_state;
    AclCacheEntry *new_entry;
    bool found;

    shared_state = get_shared_state();
    if (!shared_state || !shared_state->acl_cache)
    {
        elog(WARNING, "PostFGA: Cache insert called before initialization");
        return;
    }

    if (!entry)
    {
        elog(WARNING, "PostFGA: Invalid entry for cache insert");
        return;
    }

    LWLockAcquire(shared_state->lock, LW_EXCLUSIVE);

    new_entry = (AclCacheEntry *) hash_search(shared_state->acl_cache,
                                              &entry->key,
                                              HASH_ENTER,
                                              &found);

    if (new_entry)
    {
        memcpy(new_entry, entry, sizeof(AclCacheEntry));

        if (!found)
        {
            /* New entry added */
            stats_inc_cache_entry(&shared_state->stats);
        }
    }
    else
    {
        elog(WARNING, "PostFGA: Failed to insert cache entry (hash table full?)");
    }

    LWLockRelease(shared_state->lock);
}

/*
 * cache_delete
 *
 * Delete cache entry by key.
 *
 * Args:
 *   key - Cache key to delete
 *
 * Thread safety: Acquires exclusive lock on shared state lock
 */
void
cache_delete(const AclCacheKey *key)
{
    PostfgaShmemState *shared_state;
    AclCacheEntry *found_entry;

    shared_state = get_shared_state();
    if (!shared_state || !shared_state->acl_cache)
    {
        elog(WARNING, "PostFGA: Cache delete called before initialization");
        return;
    }

    if (!key)
    {
        elog(WARNING, "PostFGA: Invalid key for cache delete");
        return;
    }

    LWLockAcquire(shared_state->lock, LW_EXCLUSIVE);

    found_entry = (AclCacheEntry *) hash_search(shared_state->acl_cache,
                                                key,
                                                HASH_REMOVE,
                                                NULL);

    if (found_entry)
    {
        stats_inc_cache_eviction(&shared_state->stats);

        /* Decrement counter safely (avoid underflow) */
        stats_dec_cache_entry(&shared_state->stats);
    }

    LWLockRelease(shared_state->lock);
}

/*
 * cache_is_stale
 *
 * Check if cache entry is stale based on generation counters.
 *
 * Args:
 *   entry - Cache entry to check
 *
 * Returns: true if entry is stale, false if still valid
 *
 * An entry is considered stale if any of its generation counters
 * are less than the current generation for that scope.
 */
bool
cache_is_stale(const AclCacheEntry *entry)
{
    PostfgaShmemState *shared_state;
    uint64 current_gen;
    char scope_key[NAME_MAX_LEN * 2];

    shared_state = get_shared_state();

    /* Check object_type generation */
    current_gen = get_generation(entry->key.object_type);
    if (current_gen > entry->object_type_gen)
    {
        elog(DEBUG2, "PostFGA: Cache entry stale (object_type generation mismatch)");
        return true;
    }

    /* Check object generation (object_type:object_id) */
    build_scope_key(scope_key, sizeof(scope_key),
                           entry->key.object_type, entry->key.object_id);
    current_gen = get_generation(scope_key);
    if (current_gen > entry->object_gen)
    {
        elog(DEBUG2, "PostFGA: Cache entry stale (object generation mismatch)");
        return true;
    }

    /* Check subject_type generation */
    current_gen = get_generation(entry->key.subject_type);
    if (current_gen > entry->subject_type_gen)
    {
        elog(DEBUG2, "PostFGA: Cache entry stale (subject_type generation mismatch)");
        return true;
    }

    /* Check subject generation (subject_type:subject_id) */
    build_scope_key(scope_key, sizeof(scope_key),
                           entry->key.subject_type, entry->key.subject_id);
    current_gen = get_generation(scope_key);
    if (current_gen > entry->subject_gen)
    {
        elog(DEBUG2, "PostFGA: Cache entry stale (subject generation mismatch)");
        return true;
    }

    return false;
}

/*
 * cache_invalidate_by_scope
 *
 * Invalidate cache entries by incrementing generation for a scope.
 *
 * Args:
 *   scope_key - Scope key to invalidate
 *
 * Note: This is a convenience wrapper around increment_generation.
 *       Incrementing the generation will cause all cache entries with
 *       matching scope to be considered stale on next lookup.
 */
void
cache_invalidate_by_scope(const char *scope_key)
{
    if (!scope_key || scope_key[0] == '\0')
    {
        elog(WARNING, "PostFGA: Invalid scope_key for cache invalidation");
        return;
    }

    increment_generation(scope_key);

    elog(DEBUG1, "PostFGA: Invalidated cache for scope: %s", scope_key);
}
