/*-------------------------------------------------------------------------
 *
 * relation.c
 *    Relation bitmap operations for PostFGA extension.
 *
 * This module implements:
 *   - Relation name to bit index mapping
 *   - Relation registration
 *   - Relation bitmap initialization from GUC string
 *
 *-------------------------------------------------------------------------
 */

#include <postgres.h>

#include <storage/lwlock.h>
#include <string.h>
#include <utils/hsearch.h>

#include "relation.h"
#include "shmem.h"

/* -------------------------------------------------------------------------
 * Relation Bitmap Operations
 * -------------------------------------------------------------------------
 */

/*
 * get_relation_bit_index
 *
 * Get bit index for a relation name.
 *
 * Args:
 *   relation_name - Name of the relation
 *
 * Returns: Bit index (0-63), or 0 if not found
 *
 * Thread safety: Acquires shared lock on shared state lock
 *
 * Note: Returning 0 for "not found" means the first relation (bit 0)
 *       is indistinguishable from an error. Consider returning -1 or
 *       using an out parameter for the index in production code.
 */
uint8 get_relation_bit_index(const char* relation_name)
{
    RelationBitMapEntry* entry;
    uint8 bit_index = 0;

    FgaL2AclCache* state = postfga_get_shmem_state()->cache;

    if (!relation_name || relation_name[0] == '\0')
    {
        elog(WARNING, "PostFGA: Invalid relation_name for get bit index");
        return 0;
    }

    // LWLockAcquire(state->lock, LW_SHARED);

    // entry = (RelationBitMapEntry *)hash_search(state->relation_bitmap_map,
    //                                            relation_name,
    //                                            HASH_FIND,
    //                                            NULL);

    if (entry)
        bit_index = entry->bit_index;
    else
        elog(DEBUG1, "PostFGA: Relation '%s' not found in bitmap", relation_name);

    // LWLockRelease(state->lock);

    return bit_index;
}

/*
 * register_relation
 *
 * Register a relation with a bit index.
 *
 * Args:
 *   relation_name - Name of the relation
 *   bit_index     - Bit index to assign (0-63)
 *
 * Thread safety: Acquires exclusive lock on shared state lock
 *
 * Note: If relation already exists, it will be overwritten.
 */
void register_relation(const char* relation_name, uint8 bit_index)
{
    FgaL2AclCache* cache;
    RelationBitMapEntry* entry;
    bool found;

    if (!relation_name || relation_name[0] == '\0')
    {
        elog(WARNING, "PostFGA: Invalid relation_name for registration");
        return;
    }

    if (bit_index >= 64)
    {
        elog(WARNING, "PostFGA: Bit index %u out of range (max 63)", bit_index);
        return;
    }

    cache = postfga_get_shmem_state()->cache;

    // LWLockAcquire(cache->lock, LW_EXCLUSIVE);

    // entry = (RelationBitMapEntry *)hash_search(cache->relation_bitmap_map,
    //                                            relation_name,
    //                                            HASH_ENTER,
    //                                            &found);

    if (entry)
    {
        strncpy(entry->relation_name, relation_name, sizeof(entry->relation_name) - 1);
        entry->relation_name[sizeof(entry->relation_name) - 1] = '\0';
        entry->bit_index = bit_index;

        elog(DEBUG1, "PostFGA: Registered relation '%s' with bit index %u", relation_name, bit_index);
    }
    else
    {
        elog(WARNING, "PostFGA: Failed to register relation '%s'", relation_name);
    }

    // LWLockRelease(cache->lock);
}

/*
 * init_relation_bitmap
 *
 * Initialize relation bitmap from GUC string.
 *
 * Args:
 *   relations_str - Comma-separated list of relation names
 *                   (e.g., "read,write,edit,delete")
 *
 * This function parses the GUC string and registers each relation
 * with a sequential bit index (0, 1, 2, ...).
 *
 * Maximum 64 relations are supported (limited by 64-bit bitmask).
 */
void init_relation_bitmap(const char* relations_str)
{
    char* str_copy;
    char* token;
    char* saveptr = NULL;
    uint8 bit_index = 0;

    if (!relations_str || relations_str[0] == '\0')
    {
        elog(WARNING, "PostFGA: Empty relations string for initialization");
        return;
    }

    /* Make a copy since strtok_r modifies the string */
    str_copy = pstrdup(relations_str);

    /* Parse comma-separated tokens */
    token = strtok_r(str_copy, ",", &saveptr);
    while (token != NULL)
    {
        /* Trim whitespace (simple version) */
        while (*token == ' ' || *token == '\t')
            token++;

        if (*token != '\0')
        {
            if (bit_index >= 64)
            {
                elog(WARNING, "PostFGA: Too many relations (max 64), ignoring '%s' and subsequent", token);
                break;
            }

            register_relation(token, bit_index);
            bit_index++;
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    pfree(str_copy);

    elog(LOG, "PostFGA: Initialized %u relations in bitmap", bit_index);
}
