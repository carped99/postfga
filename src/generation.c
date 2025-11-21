/*-------------------------------------------------------------------------
 *
 * generation.c
 *    Generation tracking for cache invalidation.
 *
 * This module implements:
 *   - Generation counter management for different scopes
 *   - Scope key building utilities
 *   - Generation map selection logic
 *
 *-------------------------------------------------------------------------
 */

#include <postgres.h>
#include <storage/lwlock.h>
#include <utils/hsearch.h>
#include <string.h>
#include <stdio.h>

#include "generation.h"
#include "shmem.h"

/* -------------------------------------------------------------------------
 * Forward declarations (private functions)
 * -------------------------------------------------------------------------
 */

static HTAB *determine_generation_map(const char *scope_key);

/* -------------------------------------------------------------------------
 * Generation Tracking
 * -------------------------------------------------------------------------
 */

/*
 * get_generation
 *
 * Get generation counter for a scope.
 *
 * Args:
 *   scope_key - Scope key (e.g., "document" or "document:123")
 *
 * Returns: Current generation number, or 0 if not found
 *
 * Thread safety: Acquires shared lock on shared state lock
 */
uint64
get_generation(const char *scope_key)
{
    PostfgaShmemState *shared_state;
    GenerationEntry *entry;
    uint64 generation = 0;
    HTAB *gen_map;

    shared_state = postfga_get_shared_state();
    if (!shared_state)
    {
        elog(WARNING, "PostFGA: Get generation called before initialization");
        return 0;
    }

    if (!scope_key || scope_key[0] == '\0')
    {
        elog(WARNING, "PostFGA: Invalid scope_key for get generation");
        return 0;
    }

    gen_map = determine_generation_map(scope_key);
    if (!gen_map)
    {
        elog(WARNING, "PostFGA: Could not determine generation map for key: %s", scope_key);
        return 0;
    }

    LWLockAcquire(shared_state->lock, LW_SHARED);

    entry = (GenerationEntry *) hash_search(gen_map,
                                            scope_key,
                                            HASH_FIND,
                                            NULL);

    if (entry)
        generation = entry->generation;

    LWLockRelease(shared_state->lock);

    return generation;
}

/*
 * increment_generation
 *
 * Increment generation counter for a scope.
 *
 * Args:
 *   scope_key - Scope key to increment
 *
 * Thread safety: Acquires exclusive lock on shared state lock
 *
 * Side effects: Increments next_generation counter
 */
void
increment_generation(const char *scope_key)
{
    PostfgaShmemState *shared_state;
    GenerationEntry *entry;
    bool found;
    HTAB *gen_map;

    shared_state = postfga_get_shared_state();
    if (!shared_state)
    {
        elog(WARNING, "PostFGA: Increment generation called before initialization");
        return;
    }

    if (!scope_key || scope_key[0] == '\0')
    {
        elog(WARNING, "PostFGA: Invalid scope_key for increment generation");
        return;
    }

    gen_map = determine_generation_map(scope_key);
    if (!gen_map)
    {
        elog(WARNING, "PostFGA: Could not determine generation map for key: %s", scope_key);
        return;
    }

    LWLockAcquire(shared_state->lock, LW_EXCLUSIVE);

    entry = (GenerationEntry *) hash_search(gen_map,
                                            scope_key,
                                            HASH_ENTER,
                                            &found);

    if (entry)
    {
        /* Copy the key into the entry */
        if (!found)
        {
            strncpy(entry->scope_key, scope_key, sizeof(entry->scope_key) - 1);
            entry->scope_key[sizeof(entry->scope_key) - 1] = '\0';
        }

        /* Assign new generation */
        entry->generation = shared_state->next_generation++;

        elog(DEBUG2, "PostFGA: Incremented generation for '%s' to %lu",
             scope_key, (unsigned long)entry->generation);
    }
    else
    {
        elog(WARNING, "PostFGA: Failed to create generation entry for '%s'", scope_key);
    }

    LWLockRelease(shared_state->lock);
}

/*
 * build_scope_key
 *
 * Build a scope key from type and id components.
 *
 * Args:
 *   dest      - Destination buffer
 *   dest_size - Size of destination buffer
 *   type      - Type component
 *   id        - ID component (may be NULL or empty)
 *
 * Result format: "type:id" or "type" if id is empty
 */
void
build_scope_key(char *dest, size_t dest_size, const char *type, const char *id)
{
    if (!dest || dest_size == 0)
        return;

    if (!type || type[0] == '\0')
    {
        dest[0] = '\0';
        return;
    }

    if (id && id[0] != '\0')
        snprintf(dest, dest_size, "%s:%s", type, id);
    else
        snprintf(dest, dest_size, "%s", type);
}

/* -------------------------------------------------------------------------
 * Private Helper Functions
 * -------------------------------------------------------------------------
 */

/*
 * determine_generation_map
 *
 * Determine which generation map to use based on scope_key format.
 *
 * Args:
 *   scope_key - Scope key string
 *
 * Returns: Pointer to appropriate generation map, or NULL on error
 *
 * Scope key format:
 *   - "type"         -> type-level map (object_type_gen_map or subject_type_gen_map)
 *   - "type:id"      -> instance-level map (object_gen_map or subject_gen_map)
 *
 * Note: This is a heuristic. A more robust implementation might use
 *       an explicit scope type parameter.
 */
static HTAB *
determine_generation_map(const char *scope_key)
{
    PostfgaShmemState *shared_state;
    char *colon;

    shared_state = postfga_get_shared_state();
    if (!scope_key || !shared_state)
        return NULL;

    colon = strchr(scope_key, ':');

    if (colon == NULL)
    {
        /* Type-level scope - use type map */
        /* TODO: Distinguish between object and subject types */
        /* For now, use object_type_gen_map as default */
        return shared_state->object_type_gen_map;
    }
    else
    {
        /* Instance-level scope - use instance map */
        /* TODO: Distinguish between object and subject instances */
        /* For now, use object_gen_map as default */
        return shared_state->object_gen_map;
    }
}
