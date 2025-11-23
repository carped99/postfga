#ifndef OPENFGA_FDW_H
#define OPENFGA_FDW_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>
#include <fmgr.h>
#include <foreign/fdwapi.h>
#include <nodes/pg_list.h>
#include <storage/shmem.h>

/* PostgreSQL version check */
#if PG_VERSION_NUM < 180000
#error "openfga_fdw requires PostgreSQL 18.0 or later"
#endif

/* Extension information */
#define OPENFGA_FDW_VERSION "1.0.0"
#define OPENFGA_FDW_EXTENSION_NAME "openfga_fdw"
#define OPENFGA_FDW_NAMESPACE "openfga"

/* Shared memory configuration */
#define SHMEM_SIZE (1024 * 1024 * 16) /* 16 MB */
#define LWLOCK_TRANCHE_NAME "postfga"
#define LWLOCK_COUNT 1

    /* Hooks */
    extern shmem_request_hook_type prev_shmem_request_hook;
    extern shmem_startup_hook_type prev_shmem_startup_hook;

    /* Function declarations - Entry points */
    PG_FUNCTION_INFO_V1(openfga_fdw_handler);
    Datum openfga_fdw_handler(PG_FUNCTION_ARGS);

    PG_FUNCTION_INFO_V1(openfga_fdw_validator);
    Datum openfga_fdw_validator(PG_FUNCTION_ARGS);

    /* FDW callback functions */
    void openfga_fdw_get_rel_size(PlannerInfo *root, RelOptInfo *baserel,
                                  Oid foreigntableid);
    void openfga_fdw_get_paths(PlannerInfo *root, RelOptInfo *baserel,
                               Oid foreigntableid);
    ForeignScan *openfga_fdw_get_plan(PlannerInfo *root, RelOptInfo *baserel,
                                      Oid foreigntableid, ForeignPath *best_path,
                                      List *tlist, List *scan_clauses,
                                      Plan *outer_plan);
    void openfga_fdw_begin_scan(ForeignScanState *node, int eflags);
    TupleTableSlot *openfga_fdw_iterate_scan(ForeignScanState *node);
    void openfga_fdw_rescan(ForeignScanState *node);
    void openfga_fdw_end_scan(ForeignScanState *node);

    /* Cleanup function */
    void openfga_fdw_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENFGA_FDW_H */
