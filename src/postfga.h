#ifndef POSTFGA_H
#define POSTFGA_H

/* -------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------
 */
#define OBJECT_TYPE_MAX_LEN 128
#define OBJECT_ID_MAX_LEN 128
#define SUBJECT_TYPE_MAX_LEN 128
#define SUBJECT_ID_MAX_LEN 128
#define RELATION_MAX_LEN 128
#define MAX_PENDING_REQ 256

#define NAME_MAX_LEN 64

#define STORE_ID_LEN 64
#define STORE_NAME_LEN 64

/* Default hash table sizes */
#define DEFAULT_RELATION_COUNT 16
#define DEFAULT_CACHE_ENTRIES 10000
#define DEFAULT_GEN_MAP_SIZE 1024

/* -------------------------------------------------------------------------
 * Types
 * ------------------------------------------------------------------------- */
typedef struct FgaAclCacheKey
{
    uint64_t low;
    uint64_t high;
    uint16_t relation_id;
    uint16_t pad;
} FgaAclCacheKey;

#endif /* POSTFGA_H */