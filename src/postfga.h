#ifndef FGA_H
#define FGA_H

/* -------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------
 */
#define OBJECT_TYPE_MAX_LEN 64
#define OBJECT_ID_MAX_LEN 64
#define SUBJECT_TYPE_MAX_LEN 64
#define SUBJECT_ID_MAX_LEN 64
#define RELATION_MAX_LEN 64
#define MAX_PENDING_REQ 256

#define NAME_MAX_LEN 64

#define OPENFGA_STORE_ID_LEN 64
#define OPENFGA_STORE_NAME_LEN 64
#define OPENFGA_MODEL_ID_LEN 64

/* Default hash table sizes */
#define DEFAULT_RELATION_COUNT 16
#define DEFAULT_CACHE_ENTRIES 10000
#define DEFAULT_GEN_MAP_SIZE 1024

#endif /* FGA_H */