# OpenFGA FDW - Architecture Documentation

## System Overview

The OpenFGA FDW is a PostgreSQL extension that bridges PostgreSQL and OpenFGA for authorization checking. It consists of three main layers:

1. **Query Layer (FDW)**: Handles SQL queries on foreign tables
2. **Cache Layer (Shared Memory)**: Maintains high-performance permission cache
3. **Sync Layer (Background Worker + gRPC)**: Keeps cache in sync with OpenFGA

```
┌─────────────────────────────────────────────────────┐
│              PostgreSQL Backend Process             │
│                                                      │
│  ┌──────────────────────────────────────────────┐   │
│  │         FDW (Query Execution)                │   │
│  │  ┌────────────────────────────────────────┐  │   │
│  │  │  BEGIN SCAN                            │  │   │
│  │  │  ├─ Parse WHERE clause                │  │   │
│  │  │  ├─ Build cache key                   │  │   │
│  │  │  ├─ Check shared memory cache         │  │   │
│  │  │  ├─ Handle cache miss (optional gRPC) │  │   │
│  │  │  └─ Return 0 or 1 row                 │  │   │
│  │  └────────────────────────────────────────┘  │   │
│  └──────────────────────────────────────────────┘   │
│                       ↓                              │
│  ┌──────────────────────────────────────────────┐   │
│  │   Shared Memory Cache (LWLock Protected)     │   │
│  │  ┌────────────────────────────────────────┐  │   │
│  │  │  ACL Cache Hash Table                 │  │   │
│  │  │  ├─ Key: (object_type, object_id,    │  │   │
│  │  │  │        subject_type, subject_id)  │  │   │
│  │  │  └─ Value: (allow_bits, deny_bits,  │  │   │
│  │  │           generations)               │  │   │
│  │  ├────────────────────────────────────────┤  │   │
│  │  │  Generation Maps                      │  │   │
│  │  │  ├─ object_type_gen_map              │  │   │
│  │  │  ├─ object_gen_map                   │  │   │
│  │  │  ├─ subject_type_gen_map             │  │   │
│  │  │  └─ subject_gen_map                  │  │   │
│  │  ├────────────────────────────────────────┤  │   │
│  │  │  Relation Bitmap                      │  │   │
│  │  │  ├─ "read" → bit 0                   │  │   │
│  │  │  ├─ "write" → bit 1                  │  │   │
│  │  │  └─ etc.                             │  │   │
│  │  └────────────────────────────────────────┘  │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
                       ↕ (shared memory)
┌─────────────────────────────────────────────────────┐
│         Background Worker Process                   │
│                                                      │
│  ┌──────────────────────────────────────────────┐   │
│  │  Main Loop                                   │   │
│  │  1. Connect to OpenFGA gRPC                 │   │
│  │  2. Call ReadChanges stream API             │   │
│  │  3. Parse change events                     │   │
│  │  4. Update generation counters              │   │
│  │  5. Invalidate affected cache entries       │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
                       ↕ (gRPC)
┌─────────────────────────────────────────────────────┐
│              OpenFGA Service                         │
│  (Separate process/container)                       │
└─────────────────────────────────────────────────────┘
```

## Detailed Component Architecture

### 1. FDW Handler (Query Execution Layer)

**File**: `src/fdw_handler.c`, `include/openfga_fdw.h`

**Responsibilities**:
- Plan optimization for foreign table scans
- Execute filtering based on WHERE clause
- Coordinate cache lookups
- Return result tuples (0 or 1 row)

**Key Callbacks**:

```
openfga_fdw_get_rel_size()
  ├─ Estimate row count (default 1000)
  └─ Set table statistics

openfga_fdw_get_paths()
  ├─ Create ForeignPath node
  └─ Add to rel->pathlist

openfga_fdw_get_plan()
  ├─ Create ForeignScan plan
  └─ Store plan context in fdw_private

openfga_fdw_begin_scan()
  ├─ Allocate FDW state
  ├─ Parse WHERE clause
  ├─ Normalize column values
  └─ Initialize for iteration

openfga_fdw_iterate_scan()
  ├─ Build cache key from WHERE values
  ├─ Call cache_lookup()
  ├─ If miss: call gRPC if fallback enabled
  ├─ Check permission bits
  └─ Return tuple or NULL

openfga_fdw_rescan()
  ├─ Reset iteration state
  └─ Prepare for re-execution

openfga_fdw_end_scan()
  └─ Free FDW state
```

### 2. Shared Memory Cache (Cache Layer)

**File**: `src/shared_memory.c`, `include/shared_memory.h`

**Data Structures**:

```c
/* Cache Key - uniquely identifies a permission query */
struct AclCacheKey {
    char object_type[64];      // e.g., "document"
    char object_id[64];         // e.g., "doc-123"
    char subject_type[64];      // e.g., "user"
    char subject_id[64];        // e.g., "alice"
};

/* Cache Entry - stores permission bitmasks and generations */
struct AclCacheEntry {
    AclCacheKey key;

    /* Tracked generations for invalidation */
    uint64 object_type_gen;     // current gen for key.object_type
    uint64 object_gen;          // current gen for key.object_type+key.object_id
    uint64 subject_type_gen;    // current gen for key.subject_type
    uint64 subject_gen;         // current gen for key.subject_type+key.subject_id

    /* Permission bitmasks */
    uint64 allow_bits;          // bit set = relation is allowed
    uint64 deny_bits;           // bit set = relation is denied

    /* Metadata */
    TimestampTz last_updated;
    TimestampTz expire_at;
};

/* Relation bit mapping for efficient bitmask operations */
struct RelationBitMapEntry {
    char relation_name[64];     // e.g., "read"
    uint8 bit_index;            // 0-63
};

/* Shared memory state - contains all hash tables and locks */
struct AclSharedState {
    LWLock *lock;                    // Master lock

    /* Core cache */
    HTAB *acl_cache;                 // ACL cache entries
    HTAB *relation_bitmap_map;       // Relation → bit mapping

    /* Generation tracking */
    HTAB *object_type_gen_map;       // object_type → generation
    HTAB *object_gen_map;            // object_type+object_id → generation
    HTAB *subject_type_gen_map;      // subject_type → generation
    HTAB *subject_gen_map;           // subject_type+subject_id → generation

    uint64 next_generation;          // Global counter
};
```

**Key Operations**:

```
init_shared_memory()
  ├─ Request DSM space from PostgreSQL
  ├─ Initialize LWLock tranche
  └─ Called from _PG_init()

cache_lookup(key)
  ├─ Acquire lock (shared)
  ├─ Find entry in HTAB
  ├─ Check if stale:
  │  └─ Compare stored generations with current
  ├─ Return entry or NULL
  └─ Release lock

cache_insert(entry)
  ├─ Acquire lock (exclusive)
  ├─ Find or create entry
  ├─ Copy entry data
  ├─ Set last_updated timestamp
  └─ Release lock

increment_generation(scope_key)
  ├─ Acquire lock (exclusive)
  ├─ Find or create generation entry
  ├─ Increment counter
  └─ Release lock

is_cache_stale(entry)
  ├─ Check TTL expiration
  ├─ Check all generation counters
  └─ Return true if any don't match
```

**Generation-Based Invalidation Strategy**:

When a change event arrives for tuple `(OT, OID, SBT, SBID, relation)`:

```
on_change_event(OT, OID, SBT, SBID, relation):
  increment_generation("object_type:" + OT)
  increment_generation("object:" + OT + ":" + OID)
  increment_generation("subject_type:" + SBT)
  increment_generation("subject:" + SBT + ":" + SBID)

cache_entry_is_stale(entry):
  current_ot_gen = get_generation("object_type:" + entry.key.object_type)
  current_o_gen = get_generation("object:" + entry.key.object_type + ":" + entry.key.object_id)
  current_st_gen = get_generation("subject_type:" + entry.key.subject_type)
  current_s_gen = get_generation("subject:" + entry.key.subject_type + ":" + entry.key.subject_id)

  return (entry.object_type_gen != current_ot_gen ||
          entry.object_gen != current_o_gen ||
          entry.subject_type_gen != current_st_gen ||
          entry.subject_gen != current_s_gen)
```

**Benefits**:
- Granular invalidation: Changes to one user don't invalidate document caches
- Cascade coverage: Changes to object type still invalidate all objects of that type
- Memory efficient: No full table scans for invalidation
- Fast: O(1) generation increment

### 3. Background Worker (Sync Layer)

**File**: `src/background_worker.c`, `include/background_worker.h`

**Architecture**:

```
┌──────────────────────────────┐
│   PostMaster Start           │
│   ↓                          │
│   register_bgw()             │
│   ├─ Allocate BackgroundWorker │
│   ├─ Set start_time          │
│   ├─ Set restart_policy      │
│   └─ RegisterBackgroundWorker()
└──────────────────────────────┘
          ↓
┌──────────────────────────────┐
│   PostMaster forks BGW proc  │
│   ↓                          │
│   bgw_main()                 │
│   ├─ Signal handlers         │
│   ├─ BackgroundWorkerBlocks()
│   ├─ Initialize DB conn      │
│   ├─ Attach shared memory    │
│   └─ Main loop (see below)   │
└──────────────────────────────┘
```

**Main Loop**:

```c
while (!ShutdownRequestPending) {
  1. Wait with timeout (WaitLatch)
     - LATCH_SET: Signal received
     - WL_TIMEOUT: BGW_DEFAULT_NAPTIME elapsed
     - WL_POSTMASTER_DEATH: Postmaster died

  2. ResetLatch()

  3. Check signals
     - SIGTERM: Set shutdown flag
     - SIGHUP: Reload config

  4. Call bgw_process_changes()
     - Read from gRPC stream
     - Parse change events
     - Update generations in shared memory
}
```

**gRPC Integration**:

```
bgw_connect_to_openfga()
  ├─ Read endpoint from GUC
  ├─ Create gRPC channel
  ├─ Create service stub
  └─ Establish connection

bgw_process_changes()
  ├─ For each ReadChanges stream event:
  │  ├─ Parse tuple change
  │  ├─ Extract (object_type, object_id, subject_type, subject_id, relation)
  │  ├─ Call increment_generation() 4 times
  │  ├─ Optional: Prefetch fresh data via gRPC Check
  │  └─ Update cache entries
  └─ Handle stream errors (reconnect)
```

### 4. gRPC Client (OpenFGA Bridge)

**File**: `src/grpc_client.c`, `include/grpc_client.h`

**Architecture**:

```c
/* C wrapper around C++ gRPC client */

OpenFGAClient* grpc_client_create()
  ├─ Allocate client structure
  ├─ Create gRPC channel (from C++ layer)
  ├─ Create service stub (from C++ layer)
  └─ Return opaque handle

grpc_check_permission()
  ├─ Build Check request
  │  ├─ store_id (from GUC)
  │  ├─ authorization_model_id (from GUC)
  │  ├─ object: (type, id)
  │  ├─ relation: string
  │  └─ subject: (type, id)
  ├─ Call gRPC Check()
  ├─ Handle response
  │  ├─ GRPC_RESULT_ALLOW
  │  ├─ GRPC_RESULT_DENY
  │  └─ GRPC_RESULT_ERROR
  └─ Return result

grpc_read_changes()
  ├─ Initialize stream context
  ├─ Call ReadChanges() stream
  ├─ For each event:
  │  └─ Parse and return tuple components
  └─ Handle stream completion
```

**Error Handling**:

```
Network Errors:
  ├─ Connection timeout
  │  └─ Backoff + retry (exponential)
  ├─ Transient errors
  │  └─ Immediate retry
  └─ Permanent errors
     └─ Log error + continue (use stale cache)
```

### 5. GUC Configuration

**File**: `src/guc.c`, `include/guc.h`

**Configuration Hierarchy**:

```
PostgreSQL Configuration Files (postgresql.conf)
  ↓
Environment Variables
  ↓
SET command (session-level)
  ↓
Server defaults (in code)
```

**Parameters**:

```
openfga_fdw.endpoint (string, required)
  - gRPC endpoint: "dns:///host:port" or "host:port"
  - No default
  - Validated at startup

openfga_fdw.store_id (string, required)
  - OpenFGA store ID
  - No default
  - Validated at startup

openfga_fdw.authorization_model_id (string, optional)
  - Model ID override
  - Default: NULL (use store default)

openfga_fdw.relations (string, required)
  - Comma-separated relation list
  - Default: "read"
  - Used for bit mapping

openfga_fdw.cache_ttl_ms (int, optional)
  - Cache entry time-to-live in milliseconds
  - Default: 60000 (60 seconds)
  - Min: 1000, Max: 3600000

openfga_fdw.max_cache_entries (int, optional)
  - Maximum cache entries
  - Default: 10000
  - Min: 100, Max: 1000000

openfga_fdw.bgw_workers (int, optional)
  - Number of background workers
  - Default: 1
  - Min: 1, Max: 10

openfga_fdw.fallback_to_grpc_on_miss (bool, optional)
  - Call gRPC if cache miss
  - Default: true
  - false = cache-only mode (cache misses = deny)
```

## Data Flow Examples

### Example 1: Permission Check Query (Cache Hit)

```sql
SELECT 1 FROM acl_permission
WHERE object_type = 'document'
  AND object_id = '123'
  AND subject_type = 'user'
  AND subject_id = 'alice'
  AND relation = 'read';
```

**Data Flow**:

```
1. Query Executor
   └─ Call openfga_fdw_begin_scan()
      ├─ Parse WHERE clause
      ├─ Extract values: (document, 123, user, alice, read)
      └─ Allocate scan state

2. First Tuple Request
   └─ Call openfga_fdw_iterate_scan()
      ├─ Build cache key: {document, 123, user, alice}
      ├─ Call cache_lookup(key)
      │  ├─ Acquire shared lock
      │  ├─ Hash table lookup
      │  ├─ Entry found
      │  ├─ Check stale:
      │  │  ├─ stored generations vs current
      │  │  └─ Match → cache hit
      │  ├─ Release lock
      │  └─ Return entry (allow_bits=0x01)
      ├─ Check bit 0 (read):
      │  └─ allow_bits & (1<<0) = 0x01 → ALLOW
      ├─ Build result tuple (value = 1)
      └─ Return tuple

3. Next Tuple Request
   └─ Call openfga_fdw_iterate_scan()
      ├─ Return NULL (end of scan)
      └─ openfga_fdw_end_scan() called
         └─ Free state
```

**Shared Memory Access Pattern**:

```
Timeline:
  T0: FDW process acquires lock (shared)
  T0+ε: Check generation: gen_map[document] = 5, entry.object_type_gen = 5 ✓
  T0+2ε: Check generation: gen_map[document:123] = 3, entry.object_gen = 3 ✓
  T0+3ε: Retrieve allow_bits = 0x01
  T0+4ε: Release lock
```

### Example 2: Permission Check Query (Cache Miss + gRPC)

```
1. cache_lookup() returns NULL (miss)
   └─ fallback_to_grpc_on_miss = true

2. Call grpc_check_permission()
   ├─ Build Check request:
   │  ├─ store_id: "store-123"
   │  ├─ object: {type: "document", id: "123"}
   │  ├─ relation: "read"
   │  └─ subject: {type: "user", id: "alice"}
   ├─ Send to OpenFGA gRPC
   ├─ Receive response: {allowed: true}
   └─ Return GRPC_RESULT_ALLOW

3. Build cache entry
   ├─ Key: {document, 123, user, alice}
   ├─ allow_bits: 0x01 (read allowed)
   ├─ Fetch current generations:
   │  ├─ object_type_gen = 5
   │  ├─ object_gen = 3
   │  ├─ subject_type_gen = 2
   │  └─ subject_gen = 4
   └─ Insert into cache

4. Return result tuple
```

### Example 3: Background Worker Processing Change Event

```
OpenFGA Event:
  Type: TUPLE_CREATED
  Object: {type: "document", id: "456"}
  Subject: {type: "group", id: "readers"}
  Relation: "read"

BGW Processing:

1. bgw_process_changes()
   └─ grpc_read_changes() returns event tuple

2. Parse event
   ├─ object_type = "document"
   ├─ object_id = "456"
   ├─ subject_type = "group"
   ├─ subject_id = "readers"
   └─ relation = "read"

3. Update generations (4 increments)
   ├─ increment_generation("object_type:document")
   │  └─ gen_map[document]: 5 → 6
   ├─ increment_generation("object:document:456")
   │  └─ gen_map[document:456]: (new) → 1
   ├─ increment_generation("subject_type:group")
   │  └─ gen_map[group]: 1 → 2
   └─ increment_generation("subject:group:readers")
      └─ gen_map[group:readers]: 3 → 4

4. Cache invalidation triggered
   └─ Any cached entry with:
      ├─ object_type = "document" (marked stale)
      ├─ subject_type = "group" (marked stale)
      └─ Next lookup will re-fetch

5. Optional: Prefetch
   ├─ Call grpc_check_permission() with event tuple
   ├─ Create cache entry with fresh data
   ├─ Insert into cache with updated generations
   └─ Future queries hit cache immediately
```

## Lock Management

### LWLock Usage Pattern

```c
/* Read operation (non-exclusive) */
LWLockAcquire(state->lock, LW_SHARED);
{
    /* Multiple readers can hold this simultaneously */
    entry = hash_search(cache, key, HASH_FIND, NULL);
    if (entry) {
        result = *entry;  /* Copy */
    }
}
LWLockRelease(state->lock);

/* Write operation (exclusive) */
LWLockAcquire(state->lock, LW_EXCLUSIVE);
{
    /* Only one writer, readers blocked */
    entry = hash_search(cache, key, HASH_ENTER, &found);
    *entry = new_data;
}
LWLockRelease(state->lock);
```

### Contention Points

```
High Contention:
  ├─ increment_generation() in BGW
  │  └─ Multiple reads + write per event
  └─ cache_lookup() in FDW
     └─ Many concurrent reads

Mitigation:
  ├─ Keep lock critical sections short
  ├─ Copy data outside lock when possible
  └─ Use LW_SHARED when read-only
```

## Version Support Strategy

### PostgreSQL Version Handling

```c
#include "pg_config.h"

#if PG_VERSION_NUM < 180000
  #error "PostgreSQL 18.0 or later required"
#endif

#if PG_VERSION_NUM >= 190000
  /* PostgreSQL 19+ specific code */
  #define PG19_FEATURE 1
#else
  /* PostgreSQL 18 code */
  #undef PG19_FEATURE
#endif
```

### API Compatibility

| Component | PG 18 | PG 19+ | Notes |
|-----------|-------|-------|-------|
| FDW API | ✓ | ✓ | Stable interface |
| BGW API | ✓ | ✓ | No breaking changes |
| Hash tables | ✓ | ✓ | Stable interface |
| LWLock | ✓ | ✓ | Compatible |
| GUC system | ✓ | ✓ | Minor API changes |

## Performance Characteristics

### Time Complexity

```
cache_lookup(key)          O(1) average, O(n) worst case
cache_insert(entry)        O(1) amortized
increment_generation()     O(1) average, O(n) worst case
is_cache_stale(entry)      O(1)
```

### Space Complexity

```
ACL Cache:      O(cache_entries) = O(max_cache_entries)
Relation Bitmap: O(num_relations) ≤ O(64)
Generation Maps: O(unique_keys) in each map
                 ├─ object_type_gen_map: O(num_object_types)
                 ├─ object_gen_map: O(num_objects)
                 ├─ subject_type_gen_map: O(num_subject_types)
                 └─ subject_gen_map: O(num_subjects)
```

### Typical Query Latency

```
Cache Hit:
  ├─ Lock acquire: ~10-100 µs
  ├─ Hash lookup: ~100-500 ns
  ├─ Bit check: ~10 ns
  ├─ Lock release: ~10-100 µs
  └─ Total: ~20-200 µs

Cache Miss + gRPC:
  ├─ Cache lookup: ~200 µs
  ├─ gRPC call: ~10-100 ms (network-dependent)
  ├─ Cache insert: ~500 µs
  └─ Total: ~10-100 ms
```

## Future Extensions

### Planned Improvements

1. **Write Operations** (v1.1)
   - INSERT with OpenFGA API calls
   - UPDATE/DELETE support
   - Transaction coordination

2. **Advanced Caching** (v1.2)
   - Multiple cache backends (Redis, Memcached)
   - Cache preheating strategies
   - Negative caching (deny results)

3. **Monitoring** (v2.0)
   - Detailed cache statistics
   - Performance metrics
   - Custom hooks for monitoring systems

4. **Advanced Features** (v2.0)
   - Multi-tenant support
   - Custom authorization models
   - Pluggable decision strategies
