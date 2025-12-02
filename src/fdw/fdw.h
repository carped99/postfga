/* fdw.h */

#ifndef POSTFGA_FDW_H
#define POSTFGA_FDW_H

#include <postgres.h>

#include <foreign/fdwapi.h>
#include <foreign/foreign.h>
#include <nodes/pg_list.h>
#include <nodes/pathnodes.h>
#include <utils/rel.h>

/* 테이블 종류 */
typedef enum PostfgaTableKind
{
    POSTFGA_FDW_TABLE_KIND_UNKNOWN = 0,
    POSTFGA_FDW_TABLE_KIND_ACL,
    POSTFGA_FDW_TABLE_KIND_TUPLE,
    POSTFGA_FDW_TABLE_KIND_STORE,
} PostfgaTableKind;

/* foreign table / server 옵션 */
typedef struct PostfgaTableOptions
{
    PostfgaTableKind kind;
    char* endpoint; /* 필요시 사용 (/tuples, /acl 등) */
    char* base_url; /* 서버 전체 옵션 예: http://openfga:8080 */
} PostfgaTableOptions;

/* executor 상태 */
typedef struct PostfgaExecState
{
    PostfgaTableOptions* opts;
    int row; /* 데모용 row index */
    /* TODO: 여기에 BGW 채널 핸들, gRPC client 핸들 등 추가 */
} PostfgaExecState;

/* ---------- options.c ---------- */

/*
 * foreigntable OID 기준으로 옵션 파싱
 * (필요하면 FdwRoutine의 fdw_private로도 넘길 수 있음)
 */
extern PostfgaTableOptions* postfga_get_table_options(Oid foreigntableid);

/* ---------- plan.c ---------- */

extern void postfgaGetForeignRelSize(PlannerInfo* root, RelOptInfo* baserel, Oid foreigntableid);

extern void postfgaGetForeignPaths(PlannerInfo* root, RelOptInfo* baserel, Oid foreigntableid);

extern ForeignScan* postfgaGetForeignPlan(PlannerInfo* root,
                                          RelOptInfo* baserel,
                                          Oid foreigntableid,
                                          ForeignPath* best_path,
                                          List* tlist,
                                          List* scan_clauses,
                                          Plan* outer_plan);

/* ---------- exec.c ---------- */

extern void postfgaBeginForeignScan(ForeignScanState* node, int eflags);

extern TupleTableSlot* postfgaIterateForeignScan(ForeignScanState* node);

extern void postfgaReScanForeignScan(ForeignScanState* node);

extern void postfgaEndForeignScan(ForeignScanState* node);

#endif /* POSTFGA_FDW_H */
