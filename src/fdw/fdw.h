/* fdw.h */

#ifndef FGA_FDW_H
#define FGA_FDW_H

#include <postgres.h>

#include <foreign/fdwapi.h>
#include <foreign/foreign.h>
#include <nodes/pathnodes.h>
#include <nodes/pg_list.h>
#include <utils/rel.h>

#define FGA_FDW_TABLE_KIND_NAME "kind"
#define FGA_FDW_TABLE_KIND_ACL_NAME "acl"
#define FGA_FDW_TABLE_KIND_TUPLE_NAME "tuple"
#define FGA_FDW_TABLE_KIND_STORE_NAME "store"

/* 테이블 종류 */
typedef enum FgaFdwTableKind
{
    FGA_FDW_TABLE_KIND_UNKNOWN = 0,
    FGA_FDW_TABLE_KIND_ACL,
    FGA_FDW_TABLE_KIND_TUPLE,
    FGA_FDW_TABLE_KIND_STORE,
} FgaFdwTableKind;

/* foreign table / server 옵션 */
typedef struct FgaFdwTableOptions
{
    FgaFdwTableKind kind;
    char* endpoint;
    char* store_id;
    char* auth_model_id;
} FgaFdwTableOptions;
/* executor 상태 */
typedef struct FgaFdwExecState
{
    FgaFdwTableOptions* opts;
    int row; /* 데모용 row index */
    /* TODO: 여기에 BGW 채널 핸들, gRPC client 핸들 등 추가 */
} FgaFdwExecState;

/* ---------- options.c ---------- */

/*
 * foreigntable OID 기준으로 옵션 파싱
 * (필요하면 FdwRoutine의 fdw_private로도 넘길 수 있음)
 */
extern FgaFdwTableOptions* fga_get_table_options(Oid foreigntableid);

/* ---------- plan.c ---------- */

extern void fgaGetForeignRelSize(PlannerInfo* root, RelOptInfo* baserel, Oid foreigntableid);
extern void fgaGetForeignPaths(PlannerInfo* root, RelOptInfo* baserel, Oid foreigntableid);

extern ForeignScan* fgaGetForeignPlan(PlannerInfo* root,
                                      RelOptInfo* baserel,
                                      Oid foreigntableid,
                                      ForeignPath* best_path,
                                      List* tlist,
                                      List* scan_clauses,
                                      Plan* outer_plan);

/* ---------- exec.c ---------- */

extern void fgaBeginForeignScan(ForeignScanState* node, int eflags);

extern TupleTableSlot* fgaIterateForeignScan(ForeignScanState* node);

extern void fgaReScanForeignScan(ForeignScanState* node);
extern void fgaEndForeignScan(ForeignScanState* node);

#endif /* FGA_FDW_H */
